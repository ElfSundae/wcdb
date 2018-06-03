/*
 * Tencent is pleased to support the open source community by making
 * WCDB available.
 *
 * Copyright (C) 2017 THL A29 Limited, a Tencent company.
 * All rights reserved.
 *
 * Licensed under the BSD 3-Clause License (the "License"); you may not use
 * this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *       https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <WCDB/Data.hpp>
#include <WCDB/FileHandle.hpp>
#include <WCDB/Reporter.hpp>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

namespace WCDB {

FileHandle::FileHandle(const std::string &path_) : path(path_), m_fd(-1)
{
}

FileHandle::FileHandle(FileHandle &&other)
    : path(std::move(other.path)), m_fd(std::move(other.m_fd))
{
    other.m_fd = -1;
}

bool FileHandle::open(Mode mode)
{
    if (m_fd == -1) {
        int flag;
        switch (mode) {
            case Mode::ReadWrite:
                flag = O_RDWR;
                break;
            case Mode::ReadOnly:
                flag = O_RDONLY;
                break;
        }
        m_fd = ::open(path.c_str(), flag);
        if (m_fd == -1) {
            setupThreadedError();
        }
    }
    return m_fd != -1;
}

void FileHandle::close()
{
    if (m_fd != -1) {
        ::close(m_fd);
        m_fd = -1;
    }
}

ssize_t FileHandle::read(unsigned char *buffer, off_t offset, size_t size)
{
    ssize_t got;
    size_t prior = 0;
    do {
        got = pread(m_fd, buffer, size, offset);
        if (got == size) {
            break;
        }
        if (got < 0) {
            if (errno == EINTR) {
                got = 1;
                continue;
            }
            prior = 0;
            setupThreadedError();
            break;
        } else if (got > 0) {
            size -= got;
            offset += got;
            prior += got;
            buffer = got + buffer;
        }
    } while (got > 0);
    return got + prior;
}

ssize_t FileHandle::write(unsigned char *buffer, off_t offset, size_t size)
{
    ssize_t wrote;
    ssize_t prior = 0;
    do {
        wrote = pwrite(m_fd, buffer, size, offset);
        if (wrote == size) {
            break;
        }
        if (wrote < 0) {
            if (errno == EINTR) {
                wrote = 1;
                continue;
            }
            setupThreadedError();
            break;
        } else if (wrote > 0) {
            size -= wrote;
            offset += wrote;
            prior += wrote;
            buffer = wrote + buffer;
        }
    } while (wrote > 0);
    return wrote + prior;
}

void FileHandle::setupThreadedError()
{
    Error error;
    error.setSystemCode(errno, Error::Code::IOError);
    error.message = strerror(errno);
    error.infos.set("Path", path);
    Reporter::shared()->report(error);
    setThreadedError(std::move(error));
}

} //namespace WCDB
