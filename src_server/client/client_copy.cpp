//
// Created by viktor on 26.02.23.
//
#include "client_copy.h"
#include <algorithm>
#include <sstream>
#include <filesystem>
#include <cassert>

OneClient::OneClient(string& _uuid){
    m_uuid = _uuid;
    m_userName = "";
}

std::string OneClient::GetPathToFile(const File& file)
{
    auto f_ptr = FindFileInFileMap(&m_files, file);
    if(f_ptr == nullptr)
        return "";
    std::string path_from_file = std::filesystem::current_path().string();
    path_from_file.append("/");
    path_from_file.append(m_userPath);
    path_from_file.append(f_ptr->GetParentsHashs());
    path_from_file.append(f_ptr->hash);
    return path_from_file;
}

Json OneClient::GetFileJson(const File& file)
{
    std::string path_from_file = GetPathToFile(file) + ".data";

    if(std::filesystem::exists(path_from_file)) {

        std::string current_secret;
        std::ifstream reader(path_from_file, std::ios::in);
        std::ostringstream sstr;
        sstr << reader.rdbuf();
        Json newJson = JsonWorker::Deserialize(sstr.str());
        reader.close();
        return newJson;
    }
    else
        return JsonWorker::CreateJsonObject();
}

void OneClient::Init(){
    std::cout << "Init : " << m_userName << std::endl;
    FileMap temp;
    AddFileToFileMap(&temp, std::filesystem::path(m_userPath), true, true);
    for(auto el : temp)
    {
        m_files = el.second->map;
    }
    for(auto el : m_files)
    {
        el.second->parent = nullptr;
    }
}

void
OneClient::StartFile(const File &file, const std::string &secret, const std::string &parents_hash,
                     size_t file_size)
{
    if(m_userName.size() == 0)
        return;

    char key[] = ___key;

    std::filesystem::path file_path = file.path;
    std::filesystem::path file_parent_path = file.path; // example path: c:\\users\\username\\documents\\files\\1\\2\\file.txt
    file_parent_path = file_parent_path.parent_path(); // c:\\users\\username\\documents\\files\\1\\2

    std::filesystem::path t_userPath = m_userPath;
                                                            //             files/ 1 /2
    std::filesystem::path parents_hash_path{parents_hash}; // example hash: 123/456/789
    // std::filesystem::path parents_and_file_hash{parents_hash};
    // parents_and_file_hash /= file.hash; // 123/456/789/abc
    const int depth = std::distance(parents_hash_path.begin(), parents_hash_path.end()); // 3 for example hash
    File_ptr parent = nullptr;

    auto create_directory_if_not_exists = [](std::filesystem::path &path, File_ptr parent) {
        if (!std::filesystem::exists(path)) {
            std::filesystem::create_directory(path);
            std::ofstream datasavestream(path.string() + ".data", std::ios::out);
            Json          newJson = JsonWorker::CreateJsonObject();

            JsonWorker::AddToJsonVal(newJson, "link", parent->path);
            JsonWorker::AddToJsonVal(newJson, "hash", parent->hash);
            JsonWorker::AddToJsonVal(newJson, "datatime", parent->formatTime);
            JsonWorker::AddToJsonVal(newJson, "status", parent->status);
            JsonWorker::AddToJsonVal(newJson, "type", parent->type);
            JsonWorker::AddToJsonVal(newJson, "secret", "");

            auto str = JsonWorker::Serialize(newJson);
            datasavestream.write(reinterpret_cast<const char *>(str.data()), (int)str.size());
            datasavestream.close();
        }
    };

    auto origin_path_it = file_parent_path.end();
    std::advance(origin_path_it, -depth); // c:\\users\\username\\documents\\files
    for (auto hash_it = parents_hash_path.begin(); hash_it != parents_hash_path.end();
         ++hash_it, ++origin_path_it) {
        auto &one = *hash_it;

        if (one == *parents_hash_path.begin()) {
            auto iter = m_files.find(one.string());
            if (iter != std::end(m_files)) {
                parent = iter->second;
            } else {
                File    *t_new_file = new File(origin_path_it->string(), hash_it->string(),
                                               FILE_OK | FILE_LOCALE, FileType::FILE_TYPE_DIR,
                                               std::filesystem::file_time_type(), FileMap(), nullptr);
                File_ptr n_file = File_ptr(t_new_file);
                m_files[one.string()] = parent = n_file;
            }
        } else {
            auto iter = parent->map.find(one.string());
            if (iter != std::end(m_files)) {
                parent = iter->second;
            } else {
                File_ptr n_file = File_ptr(new File(
                    origin_path_it->string(), hash_it->string(), FILE_OK | FILE_LOCALE,
                    FileType::FILE_TYPE_DIR, std::filesystem::file_time_type(), FileMap(), parent));
                parent->map[one.string()] = n_file;
                parent = n_file;
            }
        }

        std::filesystem::path new_dir = t_userPath;
        new_dir /= one;

        create_directory_if_not_exists(new_dir, parent);

        t_userPath = new_dir;
    }
    m_currentParent = parent;
    m_currentFile = std::fopen((t_userPath / file_path.filename()).string().c_str(), "wb");
    // TODO(Sedenkov): error handling
    std::fseek(m_currentFile, file_size, SEEK_SET);
    std::fputc('\0', m_currentFile);
}

void OneClient::WriteChunkToCurrentFile(uint8_t *data, size_t offset, size_t size)
{
    char key[] = ___key;
    assert(m_currentFile);

    const size_t chunk_size = 4 * 1024;
    size_t current_written_size = 0;

    std::vector<uint8_t> enc_data(chunk_size);
    while (current_written_size < size) {
        std::memcpy(enc_data.data(), data + current_written_size, chunk_size);
        // decrypt
        // TODO(Sedenkov): tag check
        Decryption decrypt {
            reinterpret_cast<unsigned char *>(m_uuid.data()),
            (int)m_uuid.size(),
            (unsigned char *)m_aesKey.data(),
            const_cast<unsigned char *>(example_aes_iv),
            0, EVP_GCM_TLS_TAG_LEN
        };

        Encryption encrypt {
             reinterpret_cast<unsigned char *>(m_uuid.data()), (int)m_uuid.size(),
                  reinterpret_cast<unsigned char *>(key),
                  const_cast<unsigned char *>(example_aes_iv)
        };

        // write to file
        {
            std::lock_guard<std::mutex> client_lock(m_fileMutex);
            std::fseek(m_currentFile, offset + current_written_size, SEEK_SET);
            std::fwrite(enc_data.data(), 1, chunk_size, m_currentFile);
        }

        current_written_size += chunk_size;
    }
}

void OneClient::EndFile(const File &file, const std::string &secret, const std::string &parents_hash,
                     size_t file_size)
{
    {
        std::lock_guard<std::mutex> client_lock(m_fileMutex);
        fclose(m_currentFile);
    }

#ifdef __linux__
    chmod(m_currentPath.string(), 0660);
#endif

    std::ofstream datasavestream(m_currentPath.string() + ".data", std::ios::out);
    Json newJson = JsonWorker::CreateJsonObject();

    JsonWorker::AddToJsonVal(newJson, "link", file.path);
    JsonWorker::AddToJsonVal(newJson, "hash", file.hash);
    JsonWorker::AddToJsonVal(newJson, "datatime", file.formatTime);
    JsonWorker::AddToJsonVal(newJson, "status", file.status);
    JsonWorker::AddToJsonVal(newJson, "type", file.type);
    JsonWorker::AddToJsonVal(newJson, "secret", secret);

    auto nfile = File_ptr(new File(file));

    if(secret.size() > 0)
        nfile->status |= FILE_ENCRYPTED;
    auto str = JsonWorker::Serialize(newJson);
    //std::cout << "sonWorker::Serialize : " << str << std::endl;
    datasavestream.write(reinterpret_cast<const char*>(str.data()), (int)str.size());
    datasavestream.close();

    if(parents_hash.size() == 0)
    {
        m_files[nfile->hash] = nfile;
    }
    else
    {
        m_currentParent->map[nfile->hash] = nfile;
    }
}

void OneClient::SaveFile(const File& file, const string& secret, const string& parents_hash)
{
    char key[] = ___key;
    if(m_userName.size() == 0)
        return;

    string full_path = parents_hash + file.hash;
    //std::cout << "Save start" << std::endl;
    auto * decryptedtext = new unsigned char [m_binData.size()];
    auto * encryptedtext = new unsigned char [m_binData.size()];
    std::size_t ciphertext_len;
    //std::cout << "Save alloc buff" << std::endl;

    ::decrypt(m_binData.data(), (int)m_binData.size(),
              reinterpret_cast<unsigned char *>(m_uuid.data()), (int)m_uuid.size(),
              example_aes_tag,
              reinterpret_cast<unsigned char *>(m_aesKey.data()),
              const_cast<unsigned char *>(example_aes_iv), decryptedtext);

    ::encrypt(decryptedtext, (int)m_binData.size(),
                            reinterpret_cast<unsigned char *>(m_uuid.data()), (int)m_uuid.size(),
                            reinterpret_cast<unsigned char *>(key),
                            const_cast<unsigned char *>(example_aes_iv), encryptedtext, example_aes_tag);
    //std::cout << "Save decrypt encrypt" << std::endl;

    std::filesystem::path path_p(full_path);
    std::filesystem::path link_p(file.path);

    vector<string> split_path;
    split_path.push_back("");
    for(int i = 0; i < full_path.size(); i++)
    {
        if(full_path[i] == '\\' || full_path[i] == '/')
        {
            //std::cout << split_path.back() << std::endl;
            split_path.push_back("");
        }
        else
        {
            split_path.back() += full_path[i];
        }
    }
    vector<string> split_link;
    split_link.push_back("");
    for(int i = 0; i < file.path.size(); i++)
    {
        if(file.path[i] == '\\' || file.path[i] == '/')
        {
            //std::cout << split_link.back() << std::endl;
            split_link.push_back("");
        }
        else
        {
            split_link.back() += file.path[i];
        }
    }
    for(int i = 1; i < split_link.size(); i++)
        split_link[i] = split_link[i - 1] + (split_link[i].back() != '\\' && split_link[i - 1].back() != '\\' ? "\\" : "") + split_link[i];

    string filename = split_path.back();
    split_path.pop_back();
    split_link.pop_back();
    string t_userPath = m_userPath;
    File_ptr parent = nullptr;
    for (int i = 0, p_size = split_path.size(), l_size = split_link.size(); i < p_size; i++) {
        auto &one = split_path[i];
        std::cout << "create folder " << one.c_str() << std::endl;

        if (i == 0) { // folders in root of the user
            auto iter = m_files.find(one);
            std::cout << "Add to m_files" << std::endl;

            if (iter != std::end(m_files)) {
                std::cout << "Add exist" << std::endl;
                parent = iter->second;
            } else {
                std::cout << "Add new" << std::endl;
                File *t_new_file = new File(split_link[i - p_size + l_size], split_path[i], 9,
                                            FileType::FILE_TYPE_DIR,
                                            std::filesystem::file_time_type(), FileMap(), nullptr);
                std::cout << "Add new 2" << std::endl;
                File_ptr n_file = File_ptr(t_new_file);
                std::cout << "End Create new" << std::endl;
                m_files[one] = parent = n_file;
            }
            std::cout << "End add to m_files" << std::endl;

        } else {
            std::cout << "Add to map" << std::endl;
            auto iter = parent->map.find(one);
            if (iter != std::end(m_files))
                parent = iter->second;
            else {
                File_ptr n_file = File_ptr(new File(
                    split_link[i - p_size + l_size], split_path[i], 9, FileType::FILE_TYPE_DIR,
                    std::filesystem::file_time_type(), FileMap(), parent));
                parent->map[one] = n_file;
                parent = n_file;
            }
            std::cout << "End add to map" << std::endl;
        }
        string newDir = t_userPath + "/" + one;
        if (!std::filesystem::exists(newDir)) {
            std::filesystem::create_directory(newDir);
            std::ofstream datasavestream(newDir + ".data", std::ios::out);
            Json          newJson = JsonWorker::CreateJsonObject();

            JsonWorker::AddToJsonVal(newJson, "link", parent->path);
            JsonWorker::AddToJsonVal(newJson, "hash", parent->hash);
            JsonWorker::AddToJsonVal(newJson, "datatime", parent->formatTime);
            JsonWorker::AddToJsonVal(newJson, "status", parent->status);
            JsonWorker::AddToJsonVal(newJson, "type", parent->type);
            JsonWorker::AddToJsonVal(newJson, "secret", "");

            auto str = JsonWorker::Serialize(newJson);
            datasavestream.write(reinterpret_cast<const char *>(str.data()), (int)str.size());
            datasavestream.close();
        }
        t_userPath = newDir;
    }

    std::ofstream savestream(t_userPath + "/" + filename, std::ios::out | std::ios::binary);
    savestream.write(reinterpret_cast<const char*>(encryptedtext), (int)m_binData.size());
    savestream.close();
    m_binData.clear();

#ifdef __linux__
    chmod((t_userPath + "/" + filename).c_str(), 0660);
#endif

    std::ofstream datasavestream(t_userPath + "/" + filename + ".data", std::ios::out);
    Json newJson = JsonWorker::CreateJsonObject();

    JsonWorker::AddToJsonVal(newJson, "link", file.path);
    JsonWorker::AddToJsonVal(newJson, "hash", file.hash);
    JsonWorker::AddToJsonVal(newJson, "datatime", file.formatTime);
    JsonWorker::AddToJsonVal(newJson, "status", file.status);
    JsonWorker::AddToJsonVal(newJson, "type", file.type);
    JsonWorker::AddToJsonVal(newJson, "secret", secret);

    auto nfile = File_ptr(new File(file));

    if(secret.size() > 0)
        nfile->status |= FILE_ENCRYPTED;
    auto str = JsonWorker::Serialize(newJson);
    //std::cout << "sonWorker::Serialize : " << str << std::endl;
    datasavestream.write(reinterpret_cast<const char*>(str.data()), (int)str.size());
    datasavestream.close();

    if(split_path.size() == 0)
    {
        m_files[nfile->hash] = nfile;
    }
    else
    {
        parent->map[nfile->hash] = nfile;
    }

}

bool OneClient::CheckPermision(const File& file, const string& secret)
{
    std::string path_from_file = GetPathToFile(file) + ".data";

    if(std::filesystem::exists(path_from_file)) {

        Json newJson = GetFileJson(file);

        auto current_secret = JsonWorker::FindStringVal(newJson, "secret");

        return current_secret == secret;
    }
    else
        return secret.size() == 0;
}

void OneClient::RemoveFile(const File& file)
{
    std::string path_from_file = GetPathToFile(file);
    if(m_userName.size() == 0)
        return;
    if(std::filesystem::exists(path_from_file))
        std::remove(path_from_file.data());

    if(std::filesystem::exists(path_from_file + ".data"))
        std::remove((path_from_file + ".data").c_str());

    // TODO
    //m_files.erase(std::remove(m_files.begin(), m_files.end(), str_path));
}

void OneClient::RenameFile(const File& file_old, const File& file_new)
{
    /// TODO
    /// rework it
    ///

    return;
}
