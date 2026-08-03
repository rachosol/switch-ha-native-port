#include <stratosphere.hpp>
extern "C" {
    #include <arpa/inet.h>
    #include <switch/services/bsd.h>
    #include <switch/services/psm.h>
}

namespace ams {
    namespace native {
        namespace {
            alignas(0x40) constinit u8 g_heap_memory[64_KB];
            /* BSD normally allocates this transfer memory through libnx/newlib.
             * Stratosphere modules intentionally have no newlib heap, so retain
             * the buffer here and initialise BSD directly instead. */
            alignas(0x1000) constinit u8 g_bsd_transfer_memory[0x74000];
            /* The main system-module stack is intentionally kept free of the
             * title-cache scan. This code runs below TestMqttSession(), whose
             * MQTT buffers are still live when telemetry is published. */
            constinit char g_title_read_buffer[512];
            constinit char g_title_line[256];
            constexpr const char *StatusPath = "sdmc:/switch/.switch-ha-native-status.ini";
            constinit lmem::HeapHandle g_heap_handle;
            constinit bool g_heap_initialized;
            constinit os::SdkMutex g_heap_init_mutex;
            constinit u64 g_last_program_id;
            constinit Result g_socket_result = ResultSuccess();
            constinit Result g_psm_result = ResultSuccess();
            constinit s32 g_tcp_socket = -2;
            constinit s32 g_tcp_connect_result = -2;
            constinit s32 g_tcp_errno = 0;
            constinit s32 g_mqtt_config_result = -2;
            constinit s32 g_mqtt_connect_result = -2;
            constinit s32 g_mqtt_connack = -2;
            constinit s32 g_mqtt_publish_result = -2;
            constinit u32 g_status_cycles = 0;
            constexpr const char *TitlesPath = "sdmc:/switch/switch-ha-native/titles.txt";

            struct MqttCredentials {
                char host[64];
                char username[96];
                char password[128];
                char client_id[64];
                char discovery_prefix[96];
                u16 port;
            };

            lmem::HeapHandle GetHeapHandle() {
                if (AMS_UNLIKELY(!g_heap_initialized)) {
                    std::scoped_lock lk(g_heap_init_mutex);
                    if (AMS_LIKELY(!g_heap_initialized)) {
                        g_heap_handle = lmem::CreateExpHeap(g_heap_memory, sizeof(g_heap_memory), lmem::CreateOption_ThreadSafe);
                        g_heap_initialized = true;
                    }
                }
                return g_heap_handle;
            }
        }

        void *Allocate(size_t size) {
            return lmem::AllocateFromExpHeap(GetHeapHandle(), size);
        }

        void Deallocate(void *p, size_t size) {
            AMS_UNUSED(size);
            lmem::FreeToExpHeap(GetHeapHandle(), p);
        }

        void WriteStatus() {
            u64 pid = 0;
            u64 program_id = 0;
            const Result pid_rc = pmdmntGetApplicationProcessId(&pid);
            const Result program_rc = R_SUCCEEDED(pid_rc) ? pmdmntGetProgramId(&program_id, pid) : pid_rc;
            if (R_SUCCEEDED(program_rc) && program_id != 0) {
                g_last_program_id = program_id;
            }
            u32 battery = 0;
            PsmChargerType charger = PsmChargerType_Unconnected;
            PsmBatteryChargeInfoFields info = {};
            const Result battery_rc = R_SUCCEEDED(g_psm_result) ? psmGetBatteryChargePercentage(&battery) : g_psm_result;
            const Result charger_rc = R_SUCCEEDED(g_psm_result) ? psmGetChargerType(&charger) : g_psm_result;
            const Result info_rc = R_SUCCEEDED(g_psm_result) ? psmGetBatteryChargeInfoFields(&info) : g_psm_result;
            char text[768] = {};
            const size_t text_size = util::SNPrintf(text, sizeof(text), "format=1\nsource=native-stratosphere\nstate=running\napplication_pid=0x%016llX\napplication_program_id=0x%016llX\nlast_application_program_id=0x%016llX\npid_result=0x%08X\nprogram_result=0x%08X\nsocket_result=0x%08X\npsm_result=0x%08X\nbattery_percent=%u\nbattery_voltage_mv=%u\ncharging=%s\ntcp_socket=%d\ntcp_connect_result=%d\ntcp_errno=%d\nmqtt_config_result=%d\nmqtt_connect_result=%d\nmqtt_connack=%d\nmqtt_publish_result=%d\n", static_cast<unsigned long long>(pid), static_cast<unsigned long long>(program_id), static_cast<unsigned long long>(g_last_program_id), pid_rc.GetValue(), program_rc.GetValue(), g_socket_result.GetValue(), g_psm_result.GetValue(), R_SUCCEEDED(battery_rc) ? battery : 0, R_SUCCEEDED(info_rc) ? info.battery_charge_milli_voltage : 0, R_SUCCEEDED(charger_rc) && charger != PsmChargerType_Unconnected ? "ON" : "OFF", g_tcp_socket, g_tcp_connect_result, g_tcp_errno, g_mqtt_config_result, g_mqtt_connect_result, g_mqtt_connack, g_mqtt_publish_result);
            const auto delete_rc = fs::DeleteFile(StatusPath);
            AMS_UNUSED(delete_rc);
            if (R_FAILED(fs::CreateFile(StatusPath, text_size))) {
                return;
            }
            fs::FileHandle file;
            if (R_SUCCEEDED(fs::OpenFile(std::addressof(file), StatusPath, fs::OpenMode_Write))) {
                const auto write_rc = fs::WriteFile(file, 0, text, text_size, fs::WriteOption::Flush);
                AMS_UNUSED(write_rc);
                fs::CloseFile(file);
            }
        }

        void TestBrokerTcp(const MqttCredentials &credentials) {
            if (R_FAILED(g_socket_result)) {
                return;
            }

            const auto socket = bsdSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            g_tcp_socket = socket;
            if (socket < 0) {
                g_tcp_errno = g_bsdErrno;
                return;
            }

            sockaddr_in broker = {};
            broker.sin_family = AF_INET;
            broker.sin_port = htons(credentials.port);
            broker.sin_addr.s_addr = inet_addr(credentials.host);
            g_tcp_connect_result = bsdConnect(socket, reinterpret_cast<const sockaddr *>(std::addressof(broker)), sizeof(broker));
            g_tcp_errno = g_bsdErrno;
            bsdClose(socket);
        }

        bool LoadMqttCredentials(MqttCredentials *out) {
            *out = {};
            out->port = 1883;
            /* Compatibility with the original native deployment: its config
             * did not store the fixed broker host. A new blank template does
             * contain mqtt_host= and therefore deliberately clears this. */
            util::TSNPrintf(out->host, sizeof(out->host), "10.0.0.248");
            /* The port has its own stable identity. It remains overridable
             * through mqtt_client_id for installations that need a separate
             * device. */
            util::TSNPrintf(out->client_id, sizeof(out->client_id), "switch-ha-native");
            fs::FileHandle file;
            if (R_FAILED(fs::OpenFile(std::addressof(file), "sdmc:/switch/switch-ha-native/config.ini", fs::OpenMode_Read))) {
                return false;
            }
            ON_SCOPE_EXIT { fs::CloseFile(file); };

            s64 size = 0;
            if (R_FAILED(fs::GetFileSize(std::addressof(size), file)) || size <= 0 || size >= 1024) {
                return false;
            }

            char data[1024] = {};
            if (R_FAILED(fs::ReadFile(file, 0, data, static_cast<size_t>(size)))) {
                return false;
            }
            data[size] = '\0';

            char *line = data;
            while (line < data + size) {
                char *end = line;
                while (end < data + size && *end != '\n' && *end != '\r') ++end;
                if (end < data + size) *end++ = '\0';
                const char *value = nullptr;
                char *separator = line;
                while (*separator != '\0' && *separator != '=') ++separator;
                if (*separator == '=') {
                    *separator++ = '\0';
                    value = separator;
                    if (std::strcmp(line, "mqtt_host") == 0) {
                        util::TSNPrintf(out->host, sizeof(out->host), "%s", value);
                    } else if (std::strcmp(line, "mqtt_username") == 0) {
                        util::TSNPrintf(out->username, sizeof(out->username), "%s", value);
                    } else if (std::strcmp(line, "mqtt_password") == 0) {
                        util::TSNPrintf(out->password, sizeof(out->password), "%s", value);
                    } else if (std::strcmp(line, "mqtt_client_id") == 0) {
                        util::TSNPrintf(out->client_id, sizeof(out->client_id), "%s", value);
                    } else if (std::strcmp(line, "mqtt_topic_prefix") == 0) {
                        util::TSNPrintf(out->discovery_prefix, sizeof(out->discovery_prefix), "%s", value);
                    } else if (std::strcmp(line, "mqtt_port") == 0) {
                        u32 port = 0;
                        for (const char *p = value; *p >= '0' && *p <= '9'; ++p) port = port * 10 + static_cast<u32>(*p - '0');
                        if (port > 0 && port <= 65535) out->port = static_cast<u16>(port);
                    }
                }
                line = end;
            }
            if (out->discovery_prefix[0] == '\0') util::TSNPrintf(out->discovery_prefix, sizeof(out->discovery_prefix), "homeassistant");
            return out->host[0] != '\0' && out->username[0] != '\0' && out->password[0] != '\0';
        }

        /* titles.txt is deliberately treated as optional: a valid title ID is
         * still useful telemetry when the user has not copied the cache. */
        bool ResolveTitle(u64 program_id, char *out, size_t out_size) {
            util::TSNPrintf(out, out_size, "%016llX", static_cast<unsigned long long>(program_id));
            fs::FileHandle file;
            if (R_FAILED(fs::OpenFile(std::addressof(file), TitlesPath, fs::OpenMode_Read))) return false;
            ON_SCOPE_EXIT { fs::CloseFile(file); };

            char expected[17] = {};
            util::TSNPrintf(expected, sizeof(expected), "%016llX", static_cast<unsigned long long>(program_id));
            /* The title cache is hundreds of KiB. Scan it in small pieces so
             * this module stays inside its deliberately small 64 KiB heap. */
            s64 offset = 0;
            size_t line_length = 0;
            while (true) {
                size_t read = 0;
                if (R_FAILED(fs::ReadFile(std::addressof(read), file, offset, g_title_read_buffer, sizeof(g_title_read_buffer), fs::ReadOption())) || read == 0) break;
                offset += static_cast<s64>(read);
                for (size_t i = 0; i < read; ++i) {
                    const char c = g_title_read_buffer[i];
                    if (c == '\n' || c == '\r') {
                        if (line_length != 0) {
                            g_title_line[line_length] = '\0';
                            char *separator = g_title_line;
                            /* Existing title databases use ':', while the
                             * documented hand-edit format uses ';'. Support
                             * both so a user can extend either form. */
                            while (*separator != '\0' && *separator != ';' && *separator != ':') ++separator;
                            if (*separator == ';' || *separator == ':') {
                                *separator++ = '\0';
                                if (std::strcmp(g_title_line, expected) == 0 && separator[0] != '\0') {
                                    util::TSNPrintf(out, out_size, "%s", separator);
                                    return true;
                                }
                            }
                            line_length = 0;
                        }
                    } else if (line_length + 1 < sizeof(g_title_line)) {
                        g_title_line[line_length++] = c;
                    }
                }
            }
            /* A manually appended entry may legitimately be the final line
             * without a trailing newline. Process it as well. */
            if (line_length != 0) {
                g_title_line[line_length] = '\0';
                char *separator = g_title_line;
                while (*separator != '\0' && *separator != ';' && *separator != ':') ++separator;
                if ((*separator == ';' || *separator == ':') && separator[1] != '\0') {
                    *separator++ = '\0';
                    if (std::strcmp(g_title_line, expected) == 0) {
                        util::TSNPrintf(out, out_size, "%s", separator);
                        return true;
                    }
                }
            }
            return false;
        }

        u8 *WriteMqttString(u8 *out, const char *value) {
            const size_t length = std::strlen(value);
            *out++ = static_cast<u8>(length >> 8);
            *out++ = static_cast<u8>(length);
            std::memcpy(out, value, length);
            return out + length;
        }

        size_t EncodeRemainingLength(u8 *out, size_t value) {
            size_t count = 0;
            do {
                u8 encoded = static_cast<u8>(value % 128);
                value /= 128;
                if (value != 0) encoded |= 0x80;
                out[count++] = encoded;
            } while (value != 0 && count < 4);
            return count;
        }

        bool PublishMqtt(int socket, const char *topic, const char *payload) {
            const size_t topic_length = std::strlen(topic);
            const size_t payload_length = std::strlen(payload);
            const size_t remaining = 2 + topic_length + payload_length;
            u8 packet[512] = {};
            if (remaining + 5 > sizeof(packet)) return false;
            packet[0] = 0x31; /* retained QoS 0 */
            const size_t header = EncodeRemainingLength(packet + 1, remaining);
            u8 *ptr = packet + 1 + header;
            ptr = WriteMqttString(ptr, topic);
            std::memcpy(ptr, payload, payload_length);
            ptr += payload_length;
            return bsdSend(socket, packet, static_cast<size_t>(ptr - packet), 0) == static_cast<ssize_t>(ptr - packet);
        }

        void MakeSafeId(char *out, size_t out_size, const char *input);

        bool ClearDiscovery(int socket, const MqttCredentials &credentials, const char *component, const char *entity) {
            char safe_id[80] = {};
            char topic[192] = {};
            MakeSafeId(safe_id, sizeof(safe_id), credentials.client_id);
            util::TSNPrintf(topic, sizeof(topic), "%s/%s/%s/%s/config", credentials.discovery_prefix, component, safe_id, entity);
            return PublishMqtt(socket, topic, "");
        }

        void MakeSafeId(char *out, size_t out_size, const char *input) {
            size_t pos = 0;
            for (; input[pos] != '\0' && pos + 1 < out_size; ++pos) {
                const char c = input[pos];
                out[pos] = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ? c : '_';
            }
            out[pos] = '\0';
        }

        /* libstratosphere's compact formatter is safest with integers. PSM
         * reports both values in thousandths, so render one decimal without
         * pulling floating-point formatting into this early sysmodule. */
        void FormatMilli(char *out, size_t out_size, s64 milli) {
            const bool negative = milli < 0;
            const u64 absolute = static_cast<u64>(negative ? -milli : milli);
            util::TSNPrintf(out, out_size, "%s%llu.%01llu", negative ? "-" : "", static_cast<unsigned long long>(absolute / 1000), static_cast<unsigned long long>((absolute % 1000) / 100));
        }

        bool PublishNativeTelemetry(int socket, const MqttCredentials &credentials) {
            char safe_id[80] = {};
            char topic[192] = {};
            char payload[384] = {};
            MakeSafeId(safe_id, sizeof(safe_id), credentials.client_id);

            util::TSNPrintf(topic, sizeof(topic), "%s/sensor/%s/console_state/config", credentials.discovery_prefix, safe_id);
            util::TSNPrintf(payload, sizeof(payload), "{\"name\":\"Console State\",\"uniq_id\":\"%s_console_state\",\"stat_t\":\"switch_ha/%s/console_state\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"Nintendo Switch\",\"mf\":\"Nintendo\",\"mdl\":\"Switch\"}}", safe_id, safe_id, safe_id);
            if (!PublishMqtt(socket, topic, payload)) return false;
            util::TSNPrintf(topic, sizeof(topic), "%s/binary_sensor/%s/game_running/config", credentials.discovery_prefix, safe_id);
            util::TSNPrintf(payload, sizeof(payload), "{\"name\":\"Game Running\",\"uniq_id\":\"%s_game_running\",\"stat_t\":\"switch_ha/%s/game_running\",\"pl_on\":\"ON\",\"pl_off\":\"OFF\",\"dev\":{\"ids\":[\"%s\"]}}", safe_id, safe_id, safe_id);
            if (!PublishMqtt(socket, topic, payload)) return false;
            util::TSNPrintf(topic, sizeof(topic), "%s/sensor/%s/current_game_id/config", credentials.discovery_prefix, safe_id);
            util::TSNPrintf(payload, sizeof(payload), "{\"name\":\"Current Game ID\",\"uniq_id\":\"%s_current_game_id\",\"stat_t\":\"switch_ha/%s/current_game_id\",\"dev\":{\"ids\":[\"%s\"]}}", safe_id, safe_id, safe_id);
            if (!PublishMqtt(socket, topic, payload)) return false;
            util::TSNPrintf(topic, sizeof(topic), "%s/sensor/%s/current_game_title/config", credentials.discovery_prefix, safe_id);
            util::TSNPrintf(payload, sizeof(payload), "{\"name\":\"Current Game\",\"uniq_id\":\"%s_current_game_title\",\"stat_t\":\"switch_ha/%s/current_game_title\",\"dev\":{\"ids\":[\"%s\"]}}", safe_id, safe_id, safe_id);
            if (!PublishMqtt(socket, topic, payload)) return false;
            util::TSNPrintf(topic, sizeof(topic), "%s/sensor/%s/telemetry_heartbeat/config", credentials.discovery_prefix, safe_id);
            util::TSNPrintf(payload, sizeof(payload), "{\"name\":\"Telemetry Heartbeat\",\"uniq_id\":\"%s_telemetry_heartbeat\",\"stat_t\":\"switch_ha/%s/telemetry_heartbeat\",\"dev\":{\"ids\":[\"%s\"]}}", safe_id, safe_id, safe_id);
            if (!PublishMqtt(socket, topic, payload)) return false;
            util::TSNPrintf(topic, sizeof(topic), "%s/sensor/%s/battery_temperature/config", credentials.discovery_prefix, safe_id);
            util::TSNPrintf(payload, sizeof(payload), "{\"name\":\"Battery Temperature\",\"uniq_id\":\"%s_battery_temperature\",\"stat_t\":\"switch_ha/%s/battery_temperature\",\"unit_of_meas\":\"°C\",\"dev_cla\":\"temperature\",\"dev\":{\"ids\":[\"%s\"]}}", safe_id, safe_id, safe_id);
            if (!PublishMqtt(socket, topic, payload)) return false;
            util::TSNPrintf(topic, sizeof(topic), "%s/sensor/%s/battery_health/config", credentials.discovery_prefix, safe_id);
            util::TSNPrintf(payload, sizeof(payload), "{\"name\":\"Battery Health\",\"uniq_id\":\"%s_battery_health\",\"stat_t\":\"switch_ha/%s/battery_health\",\"unit_of_meas\":\"%%\",\"dev\":{\"ids\":[\"%s\"]}}", safe_id, safe_id, safe_id);
            if (!PublishMqtt(socket, topic, payload)) return false;
            util::TSNPrintf(topic, sizeof(topic), "%s/sensor/%s/battery/config", credentials.discovery_prefix, safe_id);
            util::TSNPrintf(payload, sizeof(payload), "{\"name\":\"Battery Level\",\"uniq_id\":\"%s_battery\",\"stat_t\":\"switch_ha/%s/battery\",\"unit_of_meas\":\"%%\",\"dev_cla\":\"battery\",\"dev\":{\"ids\":[\"%s\"]}}", safe_id, safe_id, safe_id);
            if (!PublishMqtt(socket, topic, payload)) return false;
            util::TSNPrintf(topic, sizeof(topic), "%s/sensor/%s/battery_voltage/config", credentials.discovery_prefix, safe_id);
            util::TSNPrintf(payload, sizeof(payload), "{\"name\":\"Battery Voltage\",\"uniq_id\":\"%s_battery_voltage\",\"stat_t\":\"switch_ha/%s/battery_voltage\",\"unit_of_meas\":\"mV\",\"dev_cla\":\"voltage\",\"dev\":{\"ids\":[\"%s\"]}}", safe_id, safe_id, safe_id);
            if (!PublishMqtt(socket, topic, payload)) return false;
            util::TSNPrintf(topic, sizeof(topic), "%s/binary_sensor/%s/charging/config", credentials.discovery_prefix, safe_id);
            util::TSNPrintf(payload, sizeof(payload), "{\"name\":\"Is Charging\",\"uniq_id\":\"%s_charging\",\"stat_t\":\"switch_ha/%s/charging\",\"pl_on\":\"ON\",\"pl_off\":\"OFF\",\"dev\":{\"ids\":[\"%s\"]}}", safe_id, safe_id, safe_id);
            if (!PublishMqtt(socket, topic, payload)) return false;
            u64 pid = 0, program = 0;
            const Result pid_rc = pmdmntGetApplicationProcessId(&pid);
            const Result program_rc = R_SUCCEEDED(pid_rc) ? pmdmntGetProgramId(&program, pid) : pid_rc;
            const bool active = R_SUCCEEDED(program_rc) && program != 0;
            util::TSNPrintf(topic, sizeof(topic), "switch_ha/%s/console_state", safe_id);
            if (!PublishMqtt(socket, topic, active ? "running" : "home")) return false;
            util::TSNPrintf(topic, sizeof(topic), "switch_ha/%s/game_running", safe_id);
            if (!PublishMqtt(socket, topic, active ? "ON" : "OFF")) return false;
            util::TSNPrintf(topic, sizeof(topic), "switch_ha/%s/current_game_id", safe_id);
            util::TSNPrintf(payload, sizeof(payload), active ? "%016llX" : "none", static_cast<unsigned long long>(program));
            if (!PublishMqtt(socket, topic, payload)) return false;
            util::TSNPrintf(topic, sizeof(topic), "switch_ha/%s/current_game_title", safe_id);
            if (active) ResolveTitle(program, payload, sizeof(payload)); else util::TSNPrintf(payload, sizeof(payload), "none");
            if (!PublishMqtt(socket, topic, payload)) return false;
            u32 battery = 0;
            PsmChargerType charger = PsmChargerType_Unconnected;
            PsmBatteryChargeInfoFields info = {};
            if (R_SUCCEEDED(psmGetBatteryChargePercentage(&battery))) {
                util::TSNPrintf(topic, sizeof(topic), "switch_ha/%s/battery", safe_id);
                util::TSNPrintf(payload, sizeof(payload), "%u", battery);
                if (!PublishMqtt(socket, topic, payload)) return false;
            }
            if (R_SUCCEEDED(psmGetBatteryChargeInfoFields(&info))) {
                util::TSNPrintf(topic, sizeof(topic), "switch_ha/%s/battery_voltage", safe_id);
                util::TSNPrintf(payload, sizeof(payload), "%u", info.battery_charge_milli_voltage);
                if (!PublishMqtt(socket, topic, payload)) return false;
                util::TSNPrintf(topic, sizeof(topic), "switch_ha/%s/battery_temperature", safe_id);
                FormatMilli(payload, sizeof(payload), static_cast<s64>(info.temperature_celcius));
                if (!PublishMqtt(socket, topic, payload)) return false;
                util::TSNPrintf(topic, sizeof(topic), "switch_ha/%s/battery_health", safe_id);
                FormatMilli(payload, sizeof(payload), static_cast<s64>(info.battery_age_percentage));
                if (!PublishMqtt(socket, topic, payload)) return false;
            }
            if (R_SUCCEEDED(psmGetChargerType(&charger))) {
                util::TSNPrintf(topic, sizeof(topic), "switch_ha/%s/charging", safe_id);
                if (!PublishMqtt(socket, topic, charger == PsmChargerType_Unconnected ? "OFF" : "ON")) return false;
            }
            util::TSNPrintf(topic, sizeof(topic), "switch_ha/%s/telemetry_heartbeat", safe_id);
            util::TSNPrintf(payload, sizeof(payload), "%u", g_status_cycles);
            if (!PublishMqtt(socket, topic, payload)) return false;

            /* Remove all retained discovery records produced by the retired
             * boot2 module. Native entities above intentionally replace the
             * four compatible IDs, while unsupported capabilities disappear. */
            const char *legacy_sensors[] = { "brightness", "backlight", "volume", "audio_target", "current_game", "current_game_name", "game_pid", "controller_count", "screen_rtsp_url", "screen_stream_status", "screen_rtsp_port" };
            for (const char *entity : legacy_sensors) {
                if (!ClearDiscovery(socket, credentials, std::strcmp(entity, "backlight") == 0 ? "binary_sensor" : "sensor", entity)) return false;
            }
            /* Retained discovery/state from prior builds must be explicitly
             * cleared so Home Assistant removes this unused sensor. */
            if (!ClearDiscovery(socket, credentials, "sensor", "charger_type")) return false;
            util::TSNPrintf(topic, sizeof(topic), "switch_ha/%s/charger_type", safe_id);
            if (!PublishMqtt(socket, topic, "")) return false;
            for (int player = 1; player <= 8; ++player) {
                util::TSNPrintf(payload, sizeof(payload), "player_%d_controller", player);
                if (!ClearDiscovery(socket, credentials, "sensor", payload)) return false;
            }
            return ClearDiscovery(socket, credentials, "button", "reboot") &&
                   ClearDiscovery(socket, credentials, "button", "shutdown") &&
                   ClearDiscovery(socket, credentials, "camera", "screen_camera") &&
                   ClearDiscovery(socket, credentials, "notify", "popup") &&
                   ClearDiscovery(socket, credentials, "notify", "modal");
        }

        void TestMqttSession(const MqttCredentials &credentials) {

            const auto socket = bsdSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (socket < 0) {
                g_mqtt_connect_result = -1;
                return;
            }
            ON_SCOPE_EXIT { bsdClose(socket); };

            sockaddr_in broker = {};
            broker.sin_family = AF_INET;
            broker.sin_port = htons(credentials.port);
            broker.sin_addr.s_addr = inet_addr(credentials.host);
            if (bsdConnect(socket, reinterpret_cast<const sockaddr *>(std::addressof(broker)), sizeof(broker)) != 0) {
                g_mqtt_connect_result = -1;
                return;
            }
            g_mqtt_connect_result = 0;

            u8 packet[384] = {};
            u8 variable[320] = {};
            u8 *ptr = variable;
            ptr = WriteMqttString(ptr, "MQTT");
            *ptr++ = 0x04;
            *ptr++ = 0xC2; /* clean session + username + password */
            *ptr++ = 0x00;
            *ptr++ = 30;
            ptr = WriteMqttString(ptr, credentials.client_id);
            ptr = WriteMqttString(ptr, credentials.username);
            ptr = WriteMqttString(ptr, credentials.password);
            const size_t remaining = static_cast<size_t>(ptr - variable);
            if (remaining >= 128 || remaining + 2 > sizeof(packet)) {
                g_mqtt_connect_result = -2;
                return;
            }
            packet[0] = 0x10;
            packet[1] = static_cast<u8>(remaining);
            std::memcpy(packet + 2, variable, remaining);
            if (bsdSend(socket, packet, remaining + 2, 0) != static_cast<ssize_t>(remaining + 2)) {
                g_mqtt_connect_result = -3;
                return;
            }
            u8 connack[4] = {};
            if (bsdRecv(socket, connack, sizeof(connack), 0) != sizeof(connack) || connack[0] != 0x20 || connack[1] != 0x02 || connack[2] != 0x00) {
                g_mqtt_connack = -1;
                return;
            }
            g_mqtt_connack = connack[3];
            if (g_mqtt_connack == 0) g_mqtt_publish_result = PublishNativeTelemetry(socket, credentials) ? 0 : -1;
        }
    }

    namespace init {
        void InitializeSystemModule() {
            R_ABORT_UNLESS(sm::Initialize());
            fs::InitializeForSystem();
            fs::SetAllocator(native::Allocate, native::Deallocate);
            fs::SetEnabledAutoAbort(false);
            R_ABORT_UNLESS(fs::MountSdCard("sdmc"));
            R_ABORT_UNLESS(pmdmntInitialize());
            const BsdInitConfig bsd_config = {
                .version = 9,
                .tmem_buffer = native::g_bsd_transfer_memory,
                .tmem_buffer_size = sizeof(native::g_bsd_transfer_memory),
                .tcp_tx_buf_size = 0x8000,
                .tcp_rx_buf_size = 0x8000,
                .tcp_tx_buf_max_size = 0,
                .tcp_rx_buf_max_size = 0,
                .udp_tx_buf_size = 0x2400,
                .udp_rx_buf_size = 0xA500,
                .sb_efficiency = 4,
            };
            /* 2 selects bsd:s; it matches BsdServiceType_System without
             * importing libnx's full socket-device adapter. */
            native::g_socket_result = bsdInitialize(&bsd_config, 1, 2);
            native::g_psm_result = psmInitialize();
        }

        void FinalizeSystemModule() { }
        void Startup() { }
    }

    void Main() {
        os::SetThreadNamePointer(os::GetCurrentThread(), "switch_ha_native");
        while (true) {
            /* Wi-Fi may not be usable when boot2 modules start. Retry every
             * 30 seconds until a single broker connection has succeeded. */
            const u32 cycle = native::g_status_cycles++;
            native::MqttCredentials credentials;
            if (!native::LoadMqttCredentials(std::addressof(credentials))) {
                native::g_mqtt_config_result = -1;
            } else {
                native::g_mqtt_config_result = 0;
                if (native::g_tcp_connect_result != 0 && (cycle % 6) == 0) {
                    native::TestBrokerTcp(credentials);
                }
                if (native::g_tcp_connect_result == 0 && (cycle % 6) == 1) {
                    native::TestMqttSession(credentials);
                }
            }
            native::WriteStatus();
            os::SleepThread(TimeSpan::FromSeconds(5));
        }
    }
}

void *operator new(size_t size) { return ams::native::Allocate(size); }
void *operator new(size_t size, const std::nothrow_t &) { return ams::native::Allocate(size); }
void operator delete(void *p) { ams::native::Deallocate(p, 0); }
void operator delete(void *p, size_t size) { ams::native::Deallocate(p, size); }
void *operator new[](size_t size) { return ams::native::Allocate(size); }
void *operator new[](size_t size, const std::nothrow_t &) { return ams::native::Allocate(size); }
void operator delete[](void *p) { ams::native::Deallocate(p, 0); }
void operator delete[](void *p, size_t size) { ams::native::Deallocate(p, size); }
