/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Microsoft Corporation
 *
 * -=- Robust Distributed System Nucleus (rDSN) -=-
 */

#pragma once

#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <cerrno>
#include <cstdlib>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;
#endif

namespace dsn
{
namespace dist
{

inline int run_process(const std::vector<std::string>& args)
{
    if (args.empty())
    {
        return -1;
    }

#ifdef _WIN32
    std::vector<const char*> argv;
    for (const auto& arg : args)
    {
        argv.push_back(arg.c_str());
    }
    argv.push_back(nullptr);

    return static_cast<int>(_spawnvp(_P_WAIT, args[0].c_str(), argv.data()));
#else
    std::vector<char*> argv;
    for (const auto& arg : args)
    {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    pid_t pid;
    int spawn_error = posix_spawnp(&pid, args[0].c_str(), nullptr, nullptr, argv.data(), environ);
    if (spawn_error != 0)
    {
        return spawn_error;
    }

    int status = 0;
    while (waitpid(pid, &status, 0) == -1)
    {
        if (errno != EINTR)
        {
            return errno;
        }
    }

    if (WIFEXITED(status))
    {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status))
    {
        return 128 + WTERMSIG(status);
    }
    return -1;
#endif
}

inline int read_process_output(const std::vector<std::string>& args, std::string& output)
{
    output.clear();

#ifdef _WIN32
    return -1;
#else
    if (args.empty())
    {
        return -1;
    }

    int pipe_fd[2];
    if (pipe(pipe_fd) != 0)
    {
        return errno;
    }

    posix_spawn_file_actions_t actions;
    int action_error = posix_spawn_file_actions_init(&actions);
    if (action_error != 0)
    {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return action_error;
    }

    action_error = posix_spawn_file_actions_addclose(&actions, pipe_fd[0]);
    if (action_error == 0)
    {
        action_error = posix_spawn_file_actions_adddup2(&actions, pipe_fd[1], STDOUT_FILENO);
    }
    if (action_error == 0)
    {
        action_error = posix_spawn_file_actions_addclose(&actions, pipe_fd[1]);
    }
    if (action_error != 0)
    {
        posix_spawn_file_actions_destroy(&actions);
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return action_error;
    }

    std::vector<char*> argv;
    for (const auto& arg : args)
    {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    pid_t pid;
    int spawn_error = posix_spawnp(&pid, args[0].c_str(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    if (spawn_error != 0)
    {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return spawn_error;
    }

    close(pipe_fd[1]);

    char buffer[256];
    int read_error = 0;
    while (true)
    {
        ssize_t bytes = read(pipe_fd[0], buffer, sizeof(buffer));
        if (bytes > 0)
        {
            output.append(buffer, static_cast<size_t>(bytes));
        }
        else if (bytes == 0)
        {
            break;
        }
        else if (errno != EINTR)
        {
            read_error = errno;
            break;
        }
    }

    close(pipe_fd[0]);

    int status = 0;
    while (waitpid(pid, &status, 0) == -1)
    {
        if (errno != EINTR)
        {
            return errno;
        }
    }

    if (read_error != 0)
    {
        return read_error;
    }
    if (WIFEXITED(status))
    {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status))
    {
        return 128 + WTERMSIG(status);
    }
    return -1;
#endif
}

}
}
