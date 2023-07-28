//
// Created by viktor on 26.02.23.
//

#ifndef SERVER_CLIENT_COPY_H
#define SERVER_CLIENT_COPY_H
#include "common/fileSys.h"
#include "common/json_worker.h"

#define ___key "FnK0b6VH1htWjMZ41Feg2i2VwwteUuum"

#include "src_server/additional/common_server.h"

class OneClient
{
public:
    explicit OneClient(string& _uuid);
    // ~OneClient();

    Json GetFileJson(const File& file);

    void Init();

    void StartFile(const File &file, const string &secret, const string &parents_hash, size_t file_size = 0);
    void WriteChunkToCurrentFile(uint8_t *data, size_t offset, size_t size = 0);
    void EndFile(const File &file, const std::string &secret, const std::string &parents_hash,
                     size_t file_size);

    void SaveFile(const File& file, const string& secret, const string& parents_hash);

    void RemoveFile(const File& file);

    void RenameFile(const File& file_old, const File& file_new);

    bool CheckPermision(const File& file, const string& secret);

    std::string GetPathToFile(const File& file);

    ClientConnection m_connInfo;
    ClientConnection m_connData;

    std::vector<ClientConnection> m_allDataConnections;

    vector<uint8_t> m_binData;
    FileMap m_files;

    string m_userName;
    string m_userPass;
    string m_userPath;
    string m_uuid;
    string m_aesKey;

    std::mutex m_mutex;

private:
    std::mutex m_fileMutex;
    FILE *m_currentFile = nullptr;

    File_ptr m_currentParent;
    std::filesystem::path m_currentPath;
};

#endif //SERVER_CLIENT_COPY_H
