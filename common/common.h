//
// Created by viktor on 26.02.23.
//

#ifndef SERVER_ASIO_COMMON_H
#define SERVER_ASIO_COMMON_H

// The name of the special JSON field that holds the message type for messages
#include <algorithm>
#include <regex>
#include <ranges>

#define MESSAGE_FIELD "__MESSAGE__"

enum class ECommandType
{
    SEND_FILE = 0, // upload file to the server
    RECEIVE_FILE, // download file from the server
    REMOVE_FILE,
    RENAME_FILE,
    GET_FILE_LIST
};

/* messages to the server */
enum class EClientMessageType
{
    NONE = 0,
    MERGE_SOCKETS_STEP_1,
    MERGE_SOCKETS_STEP_2,

    START_SEND_FILE, // upload
    END_SEND_FILE, // upload

    START_RECEIVE_FILE, //download

    START_REMOVE_FILE,

    START_RENAME_FILE,

    START_GET_FILES_LIST,

    /* send big file by parts */
    START_SEND_BIG_FILE,
    FILE_CHUNK_INFO,
    FILE_CHUNK,
    END_SEND_BIG_FILE,
};

/**
** @brief Messages from the server
**
*/
enum class EServerMessageType
{
    NONE = 0,

    FILE_SENT,

    START_RECEIVE_FILE,

    FILE_RECEIVED,

    FILE_REMOVED,

    FILE_RENAMED,

    USER_REG_STEP_1,

    USER_REG_STEP_2,

    PERMISSION_ERROR,

    GET_FILES_LIST,

    LOGIN_ERROR

};

static bool isNumber(const std::string& s)
{
    if (std::ranges::all_of(s.cbegin(), s.cend(), [](int ch){ return std::isdigit(ch) == 0; }))
        return false;

    return true;
}

static std::vector<std::string> split(const std::string& str, const std::string& regex_str)
{
    std::regex regexz(regex_str);
    std::vector<std::string> list(std::sregex_token_iterator(str.begin(), str.end(), regexz, -1),
                        std::sregex_token_iterator());
    return list;
}

#endif //SERVER_ASIO_COMMON_H
