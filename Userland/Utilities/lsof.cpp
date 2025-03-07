/*
 * Copyright (c) 2020, Maciej Zygmanowski <sppmacd@pm.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/ByteBuffer.h>
#include <AK/GenericLexer.h>
#include <AK/HashMap.h>
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/JsonParser.h>
#include <AK/JsonValue.h>
#include <AK/String.h>
#include <AK/Vector.h>
#include <LibCore/ArgsParser.h>
#include <LibCore/File.h>
#include <LibCore/ProcessStatisticsReader.h>
#include <ctype.h>
#include <stdio.h>

struct OpenFile {
    int fd;
    int pid;
    String type;
    String name;
    String state;
    String full_name;
};

static bool parse_name(StringView name, OpenFile& file)
{
    GenericLexer lexer(name);
    auto component1 = lexer.consume_until(':');

    if (lexer.tell_remaining() == 0) {
        file.name = component1;
        return true;
    } else {
        file.type = component1;
        auto component2 = lexer.consume_while([](char c) { return isprint(c) && !isspace(c) && c != '('; });
        lexer.ignore_while(isspace);
        file.name = component2;

        if (lexer.tell_remaining() == 0) {
            return true;
        } else {
            if (!lexer.consume_specific('(')) {
                dbgln("parse_name: expected (");
                return false;
            }

            auto component3 = lexer.consume_until(')');
            if (lexer.tell_remaining() != 0) {
                dbgln("parse_name: expected EOF");
                return false;
            }

            file.state = component3;
            return true;
        }
    }
}

static Vector<OpenFile> get_open_files_by_pid(pid_t pid)
{
    auto file = Core::File::open(String::formatted("/proc/{}/fds", pid), Core::OpenMode::ReadOnly);
    if (file.is_error()) {
        printf("lsof: PID %d: %s\n", pid, file.error().characters());
        return Vector<OpenFile>();
    }
    auto data = file.value()->read_all();

    JsonParser parser(data);
    auto result = parser.parse();

    if (!result.has_value()) {
        VERIFY_NOT_REACHED();
    }

    Vector<OpenFile> files;
    result.value().as_array().for_each([pid, &files](const JsonValue& object) {
        OpenFile open_file;
        open_file.pid = pid;
        open_file.fd = object.as_object().get("fd").to_int();

        String name = object.as_object().get("absolute_path").to_string();
        VERIFY(parse_name(name, open_file));
        open_file.full_name = name;

        files.append(open_file);
    });
    return files;
}

static void display_entry(const OpenFile& file, const Core::ProcessStatistics& statistics)
{
    printf("%-28s %4d %4d %-10s %4d %s\n", statistics.name.characters(), file.pid, statistics.pgid, statistics.username.characters(), file.fd, file.full_name.characters());
}

int main(int argc, char* argv[])
{
    if (pledge("stdio rpath proc", nullptr) < 0) {
        perror("pledge");
        return 1;
    }

    if (unveil("/proc", "r") < 0) {
        perror("unveil /proc");
        return 1;
    }

    // needed by ProcessStatisticsReader::get_all()
    if (unveil("/etc/passwd", "r") < 0) {
        perror("unveil /etc/passwd");
        return 1;
    }

    unveil(nullptr, nullptr);

    bool arg_all_processes { false };
    int arg_fd { -1 };
    const char* arg_uid { nullptr };
    int arg_uid_int = -1;
    int arg_pgid { -1 };
    pid_t arg_pid { -1 };
    const char* arg_filename { nullptr };

    if (argc == 1)
        arg_all_processes = true;
    else {
        Core::ArgsParser parser;
        parser.set_general_help("List open files of a processes. This can mean actual files in the file system, sockets, pipes, etc.");
        parser.add_option(arg_pid, "Select by PID", nullptr, 'p', "pid");
        parser.add_option(arg_fd, "Select by file descriptor", nullptr, 'd', "fd");
        parser.add_option(arg_uid, "Select by login/UID", nullptr, 'u', "login/UID");
        parser.add_option(arg_pgid, "Select by process group ID", nullptr, 'g', "PGID");
        parser.add_positional_argument(arg_filename, "Filename", "filename", Core::ArgsParser::Required::No);
        parser.parse(argc, argv);
    }
    {
        // try convert UID to int
        auto arg = String(arg_uid).to_int();
        if (arg.has_value())
            arg_uid_int = arg.value();
    }

    printf("%-28s %4s %4s %-10s %4s %s\n", "COMMAND", "PID", "PGID", "USER", "FD", "NAME");
    auto processes = Core::ProcessStatisticsReader::get_all();
    if (!processes.has_value())
        return 1;
    if (arg_pid == -1) {
        for (auto process : processes.value()) {
            if (process.key == 0)
                continue;
            auto open_files = get_open_files_by_pid(process.key);

            if (open_files.is_empty())
                continue;

            for (auto file : open_files) {
                if ((arg_all_processes)
                    || (arg_fd != -1 && file.fd == arg_fd)
                    || (arg_uid_int != -1 && (int)process.value.uid == arg_uid_int)
                    || (arg_uid != nullptr && process.value.username == arg_uid)
                    || (arg_pgid != -1 && (int)process.value.pgid == arg_pgid)
                    || (arg_filename != nullptr && file.name == arg_filename))
                    display_entry(file, process.value);
            }
        }
    } else {
        auto open_files = get_open_files_by_pid(arg_pid);

        if (open_files.is_empty())
            return 0;

        for (auto file : open_files) {
            display_entry(file, processes.value().get(arg_pid).value());
        }
    }

    return 0;
}
