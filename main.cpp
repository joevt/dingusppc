/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-24 divingkatae and maximum
                      (theweirdo)     spatium

(Contact divingkatae#1017 or powermax#2286 on Discord for more info)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

// The main runfile - main.cpp
// This is where the magic begins

#include <core/hostevents.h>
#include <core/timermanager.h>
#include <cpu/ppc/ppcemu.h>
#include <debugger/debugger.h>
#include <debugger/symbols.h>
#include <devices/deviceregistry.h>
#include <devices/common/hwcomponent.h>
#include <machines/machinefactory.h>
#include <utils/profiler.h>
#include <main.h>

#include <cinttypes>
#include <csignal>
#include <cstring>
#include <iostream>
#include <CLI11.hpp>
#include <loguru.hpp>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

using namespace std;

static void sigint_handler(int /*signum*/) {
    power_on = false;
    power_off_reason = po_signal_interrupt;
}

static void sigabrt_handler(int /*signum*/) {
    LOG_F(INFO, "Shutting down...");

    delete gMachineObj.release();
    cleanup();
}

static string appDescription = string(
    "\nDingusPPC - Alpha 1 (5/10/2024)              "
    "\nWritten by divingkatae, maximumspatium,      "
    "\njoevt, mihaip, et. al.                       "
    "\n(c) 2018-2024 The DingusPPC Dev Team.        "
    "\nThis is a build intended for testing.        "
    "\nUse at your own discretion.                  "
    "\n"
);

#ifdef CHECK_THREAD
pthread_t main_thread_id = 0;
#endif

/// Check for an existing directory (returns error message if check fails)
class WorkingDirectoryValidator : public CLI::Validator {
  public:
    WorkingDirectoryValidator() : Validator("DIR") {
        func_ = [](std::string &filename) {
            auto path_result = CLI::detail::check_path(filename.c_str());
            if(path_result == CLI::detail::path_type::nonexistent) {
                return "Directory does not exist: " + filename;
            }
            if(path_result == CLI::detail::path_type::file) {
                return "Directory is actually a file: " + filename;
            }
            chdir(filename.c_str());
            return std::string();
        };
    }
};

const WorkingDirectoryValidator WorkingDirectory;

void run_machine(std::string machine_str, char *rom_data, size_t rom_size, uint32_t execution_mode, uint32_t profiling_interval_ms);

int main(int argc, char** argv) {

#ifdef CHECK_THREAD
    main_thread_id = pthread_self();
#endif

    uint32_t execution_mode = interpreter;

    CLI::App app(appDescription);
    app.allow_windows_style_options(); /* we want Windows-style options */
    app.allow_extras();

    bool   realtime_enabled = false;
    bool   debugger_enabled = false;
    string bootrom_path("bootrom.bin");
    string symbols_path;
    string working_directory_path(".");
    vector<string> machine_list;

    auto execution_mode_group = app.add_option_group("execution mode")
        ->require_option(-1);
    execution_mode_group->add_flag("-r,--realtime", realtime_enabled,
        "Run the emulator in real-time");
    execution_mode_group->add_flag("-d,--debugger", debugger_enabled,
        "Enter the built-in debugger");
    app.add_option("-w,--workingdir", working_directory_path, "Specifies working directory")
        ->check(WorkingDirectory);
    app.add_option("-b,--bootrom", bootrom_path, "Specifies BootROM path")
        ->check(CLI::ExistingFile);

    bool              log_to_stderr = false;
    loguru::Verbosity log_verbosity = loguru::Verbosity_INFO;
    bool              log_no_uptime = false;
    app.add_flag("--log-to-stderr", log_to_stderr,
        "Send internal logging to stderr (instead of dingusppc.log)");
    app.add_flag("--log-verbosity", log_verbosity,
        "Adjust logging verbosity (default is 0 a.k.a. INFO)")
        ->check(CLI::Number);
    app.add_flag("--log-no-uptime", log_no_uptime,
        "Disable the uptime preamble of logged messages");

    uint32_t profiling_interval_ms = 0;
#ifdef CPU_PROFILING
    app.add_option("--profiling-interval-ms", profiling_interval_ms,
        "Specifies periodic interval (in ms) at which to output CPU profiling information");
#endif

    string       machine_str;
    CLI::Option* machine_opt = app.add_option("-m,--machine",
        machine_str, "Specify machine ID");

    app.add_option("-s,--symbols", symbols_path, "Specifies symbols path")
        ->check(CLI::ExistingFile);

    auto list_cmd = app.add_subcommand("list",
        "Display available machine configurations and exit")
        ->require_subcommand();

    auto machines = list_cmd->add_subcommand("machines", "List supported machines");
    auto properties = list_cmd->add_subcommand("properties", "List available properties");
    properties->add_option("machine", machine_list, "machine to list");

    CLI11_PARSE(app, argc, argv);

    if (*list_cmd) {
        if (*machines)
            MachineFactory::list_machines();
        if (*properties)
            MachineFactory::list_properties(machine_list);
        return 0;
    }

    if (debugger_enabled) {
        execution_mode = debugger;
    }

    /* initialize logging */
    loguru::g_preamble_date    = false;
    loguru::g_preamble_time    = false;
    loguru::g_preamble_thread  = false;
    loguru::g_preamble_uptime  = !log_no_uptime;

    if (execution_mode == interpreter && !log_to_stderr) {
        loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
        loguru::init(argc, argv);
        loguru::add_file("dingusppc.log", loguru::Append, log_verbosity);
    } else {
        loguru::g_stderr_verbosity = log_verbosity;
        loguru::init(argc, argv);
    }

    auto rom_data = std::unique_ptr<char[]>(new char[4 * 1024 * 1024]);
    memset(&rom_data[0], 0, 4 * 1024 * 1024);
    size_t rom_size = MachineFactory::read_boot_rom(bootrom_path, &rom_data[0]);
    if (!rom_size) {
        return 1;
    }

    string machine_str_from_rom = MachineFactory::machine_name_from_rom(&rom_data[0], rom_size);
    if (machine_str_from_rom.empty()) {
        LOG_F(ERROR, "Could not autodetect machine from ROM.");
    } else {
        LOG_F(INFO, "Machine detected from ROM as: %s", machine_str_from_rom.c_str());
    }
    if (*machine_opt) {
        LOG_F(INFO, "Machine option was passed in: %s", machine_str.c_str());
    } else {
        machine_str = machine_str_from_rom;
    }
    if (machine_str.empty()) {
        LOG_F(ERROR, "Must specificy a machine or provide a supported ROM.");
        return 1;
    }

    if (symbols_path.length()) {
        load_symbols(symbols_path);
    }

#if 1
    /* handle overriding of machine settings from command line */
    LOG_F(INFO, "Getting machine settings:");
    if (MachineFactory::get_machine_settings(machine_str) < 0) {
        return 1;
    }
#endif

    CLI::App sa;
    sa.allow_extras();

#if 1
    for (auto& s : gMachineFactorySettings) {
        sa.add_option("--" + s.first, s.second);
    }
#endif

#if 0
    /* handle overriding of machine settings from command line for
       devices that may be added during create_machine_for_id below */
    LOG_F(INFO, "Getting other settings:");
    for (auto& r : DeviceRegistry::get_registry()) {
        for (auto& p : r.second.properties) {
            if (!gMachineFactorySettings.count(p.first)) {
                LOG_F(INFO, "Adding setting \"%s\" = \"%s\" from %s.", p.first.c_str(), p.second->get_string().c_str(), r.first.c_str());
                gMachineFactorySettings[p.first] = p.second->get_string();
                gMachineSettings[p.first] = unique_ptr<BasicProperty>(p.second->clone());
                sa.add_option("--" + p.first, gMachineFactorySettings[p.first]);
            }
        }
    }
#endif

    sa.parse(app.remaining_for_passthrough()); /* TODO: handle exceptions! */

    MachineFactory::set_machine_settings();

    cout << "BootROM path: " << bootrom_path << endl;
    cout << "Execution mode: " << execution_mode << endl;

    if (!init()) {
        LOG_F(ERROR, "Cannot initialize");
        return 1;
    }

    // initialize global profiler object
    gProfilerObj.reset(new Profiler());

    // graceful handling of fatal errors
    loguru::set_fatal_handler([](const loguru::Message& message) {
        // Make sure the reason for the failure is visible (it may have been
        // sent to the logfile only).
        cerr << message.preamble << message.indentation << message.prefix << message.message << endl;
        power_off_reason = po_enter_debugger;
        enter_debugger();

        // Ensure that NVRAM and other state is persisted before we terminate.
        delete gMachineObj.release();
    });

    // redirect SIGINT to our own handler
    signal(SIGINT, sigint_handler);

    // redirect SIGABRT to our own handler
    signal(SIGABRT, sigabrt_handler);

    while (true) {
        run_machine(machine_str, &rom_data[0], rom_size, execution_mode, profiling_interval_ms);
        if (power_off_reason == po_restarting) {
            LOG_F(INFO, "Restarting...");
            power_on = true;
            continue;
        }
        if (power_off_reason == po_shutting_down) {
            if (execution_mode != debugger) {
                LOG_F(INFO, "Shutdown.");
                break;
            }
            LOG_F(INFO, "Shutdown...");
            power_on = true;
            continue;
        }
        break;
    }

    cleanup();

    return 0;
}

void run_machine(std::string machine_str, char *rom_data, size_t rom_size, uint32_t execution_mode,
    uint32_t
#ifdef CPU_PROFILING
    profiling_interval_ms
#endif
) {
    if (MachineFactory::create_machine_for_id(machine_str, rom_data, rom_size) < 0) {
        return;
    }

    // set up system wide event polling using
    // default Macintosh polling rate of 11 ms
    uint32_t event_timer = TimerManager::get_instance()->add_cyclic_timer(MSECS_TO_NSECS(11), [] {
        EventManager::get_instance()->poll_events();
    });

#ifdef CPU_PROFILING
    uint32_t profiling_timer;
    if (profiling_interval_ms > 0) {
        profiling_timer = TimerManager::get_instance()->add_cyclic_timer(MSECS_TO_NSECS(profiling_interval_ms), [] {
            gProfilerObj->print_profile("PPC_CPU");
        });
    }
#endif

    switch (execution_mode) {
    case interpreter:
        power_off_reason = po_starting_up;
        enter_debugger();
        break;
    case threaded_int:
        power_off_reason = po_starting_up;
        enter_debugger();
        break;
    case debugger:
        power_off_reason = po_enter_debugger;
        enter_debugger();
        break;
    default:
        LOG_F(ERROR, "Invalid EXECUTION MODE");
        return;
    }

    LOG_F(INFO, "Cleaning up...");
    TimerManager::get_instance()->cancel_timer(event_timer);
#ifdef CPU_PROFILING
    if (profiling_interval_ms > 0) {
        TimerManager::get_instance()->cancel_timer(profiling_timer);
    }
#endif
    EventManager::get_instance()->disconnect_handlers();
    delete gMachineObj.release();
}
