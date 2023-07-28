//
// Created by viktor on 25.02.23.
//

#include <filesystem>
#include "WebsocketClient.h"
#include "uploader.h"
#include <boost/bind/bind.hpp>
#include "websocketpp/common/system_error.hpp"
#include <common/asymmetric.h>

#include "common/WLoger/Include/WLoger.h"

#define BIG_FILE_TRESHOLD 10 /* mb */ * 1024 * 1024
constexpr int NumberOfSecondaryConnections = 7;
constexpr int NumberOfConnections = NumberOfSecondaryConnections + 1;
// TODO(Sedenkov): potential thread pool for tasks?
constexpr int MaxNumberOfThreads = NumberOfSecondaryConnections + 1;

// sockjs emulation
void WebsocketClient::websocket_on_http_mes(websocketpp::connection_hdl hdl) {
    auto con = m_wsClientInfo.get_con_from_hdl(hdl);

    message_ = "HTTP mes";
    WLI << "HTTP mes";

	con->set_status(websocketpp::http::status_code::ok);
	con->append_header("access-control-allow-origin", "*");
}

void WebsocketClient::websocket_on_http_dat(websocketpp::connection_hdl hdl) {
    auto con = m_wsClientData.get_con_from_hdl(hdl);

    message_ = "HTTP dat";
    WLI << "HTTP dat";

	con->set_status(websocketpp::http::status_code::ok);
	con->append_header("access-control-allow-origin", "*");
}

std::string WebsocketClient::message()
{
    auto temp = message_;
    message_ = "";
    return temp;
}

void WebsocketClient::SetFileUploader(FileUploader* var) { uploader = var; }

void WebsocketClient::SetPassword(const char* pass) {
    pass_ = pass;
}

void WebsocketClient::SetUsername(const char* user) {
    user_ = user;
}

void WebsocketClient::Encrypt(void *input_data, void* output_data, size_t data_size, void* key)
{
    ::encrypt(
        (unsigned char*)input_data,
        data_size,
        reinterpret_cast<unsigned char *>(m_uuid.data()),
        (int)m_uuid.size(),
        (unsigned char*)key,
        const_cast<unsigned char *>(example_aes_iv),
        (unsigned char*)output_data,
        example_aes_tag);
}

void WebsocketClient::Decrypt(void *input_data, void* output_data, size_t data_size, void* key)
{
    ::decrypt(
        (unsigned char*)input_data,
        data_size,
        reinterpret_cast<unsigned char *>(m_uuid.data()),
        (int)m_uuid.size(),
        example_aes_tag,
        (unsigned char*)key,
        const_cast<unsigned char *>(example_aes_iv),
        (unsigned char*)output_data);
}

WebsocketClient::WebsocketClient(string& uri_info, string& uri_data)
{
    message_ = "";

    m_uriInfo = uri_info;
    m_uriData = uri_data;

    /* binding info socket */
    m_wsClientInfo.set_open_handler([this](auto && PH1) { onOpenInfo(std::forward<decltype(PH1)>(PH1)); });
    m_wsClientInfo.set_fail_handler([this](auto && PH1) { onFailInfo(std::forward<decltype(PH1)>(PH1)); });
    m_wsClientInfo.set_message_handler([this](auto && PH1, auto && PH2) { onMessageInfo(std::forward<decltype(PH1)>(PH1),
                                                                          std::forward<decltype(PH2)>(PH2)); });
    m_wsClientInfo.set_close_handler([this](auto && PH1) { onCloseInfo(std::forward<decltype(PH1)>(PH1)); });
    m_wsClientInfo.set_http_handler([this](auto && PH1) { websocket_on_http_mes(std::forward<decltype(PH1)>(PH1)); });

    m_wsClientInfo.init_asio(&(this->eventLoop));

    /* binding data socket */
    m_wsClientData.set_open_handler([this](auto && PH1) { onOpenData(std::forward<decltype(PH1)>(PH1)); });
    m_wsClientData.set_fail_handler([this](auto && PH1) { onFailData(std::forward<decltype(PH1)>(PH1)); });
    m_wsClientData.set_message_handler([this](auto && PH1, auto && PH2) { onMessageData(std::forward<decltype(PH1)>(PH1),
                                                                          std::forward<decltype(PH2)>(PH2)); });
    m_wsClientData.set_close_handler([this](auto && PH1) { onCloseData(std::forward<decltype(PH1)>(PH1)); });
    m_wsClientData.set_http_handler([this](auto && PH1) { websocket_on_http_dat(std::forward<decltype(PH1)>(PH1)); });

    m_wsClientData.init_asio(&(this->eventLoop));
    message_ = "client created";
}

void WebsocketClient::run()
{
    /* connect info socket */
    websocketpp::lib::error_code ec;
    ws_client::connection_ptr con = m_wsClientInfo.get_connection(m_uriInfo, ec);
    m_wsClientInfo.connect(con);

    /* connect data socket */
    websocketpp::lib::error_code ec_sec;
    ws_client::connection_ptr con_sec = m_wsClientData.get_connection(m_uriData, ec_sec);
    m_wsClientData.connect(con_sec);

    for (int i = 0; i < NumberOfSecondaryConnections; i++) {
        ws_client::connection_ptr con_sec = m_wsClientData.get_connection(m_uriData, ec_sec);
        m_wsClientData.connect(con_sec);
    }

    eventLoopThread = new std::thread(boost::bind(&asio::io_context::run, &eventLoop));
    message_ = "client runned";
    //auto ret = eventLoop.run();
}

void WebsocketClient::close() {
    message_ = "starting close connection";
    websocketpp::lib::error_code ec;
    m_wsClientInfo.close(m_connInfo, websocketpp::close::status::normal, "", ec);
    m_wsClientData.close(m_connData, websocketpp::close::status::normal, "", ec);
    eventLoop.stop();
    eventLoopThread->join();
    delete eventLoopThread;
    message_ = "ending close connection";
}

string WebsocketClient::generate_secret(const string& password)
{
    string secret_hash = sha256_string(password);
    unsigned char key[SHA256_DIGEST_LENGTH];
    sha256_hash(password + password, key);
    for(int i = 0; i < 16; i++)
        std::swap(secret_hash[i * 4], secret_hash[i * 4 + 2]);

    /* encrypt hash */
    auto * encryptedtext = new unsigned char [secret_hash.size()];

    Encrypt(secret_hash.data(), encryptedtext, secret_hash.size(), key);

    string secret = string(secret_hash.size() * 2, '0');

    for (int i = 0; i < secret_hash.size(); i++) {
        sprintf(secret.data() + (i * 2), "%02x", encryptedtext[i]);
    }

    return secret;
}

void
WebsocketClient::SendBigFile(std::filesystem::path path_from_file, Json &options, const string &password)
{
    const size_t file_size = std::filesystem::file_size(path_from_file);
    std::string filename = path_from_file.filename().string();
    const int max_threads = MaxNumberOfThreads;

    const int chunk_size = 64 * 1024;  // 4, 8, 16, 32, 64 kb

    FILE *f;
    f = fopen(path_from_file.string().c_str(), "rb");
    assert(f);

#if 0
    FILE *test_f;
    std::mutex test_file_mutex;
    test_f = fopen((path_from_file.string() + "decrypt").c_str(), "wb");
    assert(test_f);
#endif

    const int remainder = file_size % chunk_size;
    const int blocks_needed =
        static_cast<int>(std::ceil(static_cast<double>(file_size) / chunk_size));
    const int blocks_per_thread = blocks_needed / max_threads;

    // Determine the actual block size (divisible by chunk_size)
    const size_t block_size = blocks_per_thread * chunk_size; //there we find out how many blocks need for one thread

    unsigned char key[SHA256_DIGEST_LENGTH];
    sha256_hash(password, key);

    std::mutex file_mutex;
    std::mutex client_mutex;

    std::vector<std::future<void>> async_sends;

    // send total size and number of chunks?

    for (int i = 0; i < max_threads; i++) {
        size_t offset = i * block_size;
        size_t read_size = (i == max_threads - 1) ? (file_size - offset) : block_size;

        async_sends.push_back(
            std::async(std::launch::async, [this, &f,filename, offset, chunk_size, block_size, read_size, i,
                                            &file_mutex, &client_mutex, key,
                                            &connection = m_allDataConnections[i]]() mutable {
                auto info = JsonWorker::CreateJsonObject();
                JsonWorker::AddToJsonVal(info, "offset", 0);
                JsonWorker::AddToJsonVal(info, MESSAGE_FIELD,
                                         (int)EClientMessageType::FILE_CHUNK_INFO);

                // std::unique_ptr<uint8_t> chunk(new uint8_t[chunk_size]);
                std::unique_ptr<uint8_t> encrypted_chunk(new uint8_t[chunk_size]);

                // size_t current_size = 0;
                while (read_size > 0) {
                    size_t current_chunk_size = std::min(read_size, (size_t)chunk_size);

                    JsonWorker::ChangeVal(info, "offset", std::to_string(offset));
                    auto json_str = JsonWorker::Serialize(info);
                    // NOTE(Sedenkov): additional 1 for null termination for separating json and bin
                    // data
                    size_t total_payload_size = json_str.size() + 1 + current_chunk_size;
                    std::unique_ptr<uint8_t> payload(new uint8_t[total_payload_size]);
                    std::memcpy(payload.get(), json_str.c_str(), json_str.size() + 1);

                    uint8_t *chunk = payload.get() + json_str.size() + 1;

                    Encryption encrypt1 {
                        reinterpret_cast<unsigned char *>(m_uuid.data()),
                        (int)m_uuid.size(),
                        (unsigned char *)key,
                        const_cast<unsigned char *>(example_aes_iv),
                    };

                    Encryption encrypt2 {
                        reinterpret_cast<unsigned char *>(m_uuid.data()),
                        (int)m_uuid.size(),
                        (unsigned char *)m_aesKey.data(),
                        const_cast<unsigned char *>(example_aes_iv),
                    };

                    /* read chunk from file */
                    {
                        std::lock_guard<std::mutex> file_lock(file_mutex);
                        fseek(f, offset, SEEK_SET);
                        fread(chunk, 1, current_chunk_size, f);
                    }

                    /* first encrypt */
                    encrypt1.EncyptNextBlock(chunk, current_chunk_size, encrypted_chunk.get());

                    unsigned char tag1[EVP_GCM_TLS_TAG_LEN];
                    encrypt1.FinishEncryption(encrypted_chunk.get(), tag1);

                    /* second encrypt */
                    encrypt2.EncyptNextBlock(encrypted_chunk.get(), current_chunk_size, chunk);

                    unsigned char tag2[EVP_GCM_TLS_TAG_LEN];
                    encrypt2.FinishEncryption(chunk, tag2);
#if 0 // test decryption
                    Decryption decrypt1 {reinterpret_cast<unsigned char *>(m_uuid.data()),
                                         (int)m_uuid.size(),
                                         (unsigned char *)key,
                                         const_cast<unsigned char *>(example_aes_iv),
                                         tag1,
                                         EVP_GCM_TLS_TAG_LEN};
                    Decryption decrypt2 {reinterpret_cast<unsigned char *>(m_uuid.data()),
                                         (int)m_uuid.size(),
                                         (unsigned char *)m_aesKey.data(),
                                         const_cast<unsigned char *>(example_aes_iv),
                                         tag2,
                                         EVP_GCM_TLS_TAG_LEN};
                    std::unique_ptr<uint8_t> decrypt_chunk1(new uint8_t[current_chunk_size]);
                    std::unique_ptr<uint8_t> decrypt_chunk2(new uint8_t[current_chunk_size]);
                    decrypt2.DecryptNextBlock(decrypt_chunk1.get(), current_chunk_size, chunk);
                    decrypt2.FinishDecryption(decrypt_chunk1.get(), tag2);
                    decrypt1.DecryptNextBlock(decrypt_chunk2.get(), current_chunk_size, decrypt_chunk1.get());
                    decrypt1.FinishDecryption(decrypt_chunk2.get(), tag1);
                    {
                        std::lock_guard<std::mutex> test_file_lock(test_file_mutex);
                        fseek(test_f, offset, SEEK_SET);
                        fwrite(decrypt_chunk2.get(), 1, current_chunk_size, test_f);
                    }
#endif

                    websocketpp::lib::error_code ec;
                    {
                        std::lock_guard<std::mutex> client_lock(client_mutex);
                        m_wsClientData.send(connection, payload.get(), total_payload_size,
                                            websocketpp::frame::opcode::binary, ec);
                    }

                    read_size -= current_chunk_size;
                    offset += current_chunk_size;
                    float ans = static_cast<float>(offset)/static_cast<float>(read_size+offset);
                    uploader->SetLoadProgress(filename, ans);
                }
            }));
    }

    // Wait for all the threads to finish
    for (auto &future: async_sends) {
        future.get();
    }
    fclose(f);
}




/** sending file */
void WebsocketClient::commandSendFile(Json& options, const string& password)
{
    string file = JsonWorker::FindStringVal(options, "file");

    string parents_hashs;
    File tfile;
    tfile.FromStringNoMap(file, parents_hashs);
    
    string path_from_file = tfile.path;

    if (!path_from_file.empty())
    {

        size_t file_size = std::filesystem::file_size(path_from_file);

        // NOTE(Sedenkov): I want to use it always
        if (true || file_size > BIG_FILE_TRESHOLD) {
            JsonWorker::AddToJsonVal(options, "file_size", file_size);
            sendMessageInfo(m_connInfo, EClientMessageType::START_SEND_BIG_FILE, options);

            // NOTE(Sedenkov): use FILE* for better performance

            SendBigFile(path_from_file, options, password);

            JsonWorker::ChangeVal(options, MESSAGE_FIELD,
                                  std::to_string((int)EClientMessageType::END_SEND_BIG_FILE));

            websocketpp::lib::error_code ec;
            m_wsClientData.send(m_connData, JsonWorker::Serialize(options),
                                websocketpp::frame::opcode::text, ec);
        } else {
            // send info about the start of sending the file
            sendMessageInfo(m_connInfo, EClientMessageType::START_SEND_FILE, options);

            /* send binary data of file */
            std::ifstream   input(path_from_file, std::ios::binary);
            vector<uint8_t> m_data(std::istreambuf_iterator<char>(input), {});
            int             size = m_data.size();

            m_data.push_back(0);

            if (password.size() > 0) {
                unsigned char key[SHA256_DIGEST_LENGTH];
                sha256_hash(password, key);
                vector<uint8_t> t_data = m_data;
                Encrypt(t_data.data(), m_data.data(), size, key);
            }

            /* Buffer for the decrypted text */
            auto       *encryptedtext = new unsigned char[size];
            std::size_t ciphertext_len;

            /* encrypt data */
            ciphertext_len = size;
            Encrypt(m_data.data(), encryptedtext, size, m_aesKey.data());

            /* sending data */
            size_t                       pointer = 0, mess_size = 1024, curLen;
            websocketpp::lib::error_code ec;
            size_t                       ___TTTT___ = 0;
            while (pointer != ciphertext_len) {
                ___TTTT___++;
                if (___TTTT___ >= 20) {
                    ___TTTT___ = 0;
                    WLI << "commandSendFile. pointer = " << pointer / 1024.0 / 1024.0 << "MB";
                }
                curLen = std::min(mess_size, ciphertext_len - pointer);
                m_wsClientData.send(m_connData, encryptedtext + pointer, curLen,
                                    websocketpp::frame::opcode::binary, ec);
                pointer += curLen;
                float ans = static_cast<float>(pointer)/static_cast<float>(ciphertext_len);
                std::string filename = std::filesystem::path(path_from_file).filename().string();
                uploader->SetLoadProgress(filename, ans);
                //  m_wsClientData.
            }

            delete[] encryptedtext;

            /* send info about the end of sending the file */

            JsonWorker::ChangeVal(options, MESSAGE_FIELD,
                                  std::to_string((int)EClientMessageType::END_SEND_FILE));

            m_wsClientData.send(m_connData, JsonWorker::Serialize(options),
                                websocketpp::frame::opcode::text, ec);
        }
    }

}

/** save receiving file from server */
void WebsocketClient::saveFile(const string& str_path, const string& hash)
{
    int size = m_binData.size();
    m_binData.push_back(0);
    auto * decryptedtext = new unsigned char [size + 1];
    decryptedtext[size] = 0;
    std::string password = temporal_password[hash];
    temporal_password.erase(hash);
    /* decrypt data */
    Decrypt(m_binData.data(), decryptedtext, size, m_aesKey.data());

    if(password.size() > 0) {
        unsigned char key[SHA256_DIGEST_LENGTH];
        sha256_hash(password, key);
        auto * new_decryptedtext = new unsigned char [size + 1];
        new_decryptedtext[size] = 0;

        Decrypt(decryptedtext, new_decryptedtext, size, key);

        delete[] decryptedtext;
        decryptedtext = new_decryptedtext;
    }

    /* save data */
    string directory;
    size_t last_slash_idx = str_path.rfind('\\');
    if (std::string::npos == last_slash_idx)
    {
        last_slash_idx = str_path.rfind('/');
    }
    if (std::string::npos != last_slash_idx)
    {
        directory = str_path.substr(0, last_slash_idx);
    }
    std::filesystem::create_directory(directory);
    std::ofstream savestream(str_path, std::ios::out | std::ios::binary);
    savestream.write(reinterpret_cast<const char*>(decryptedtext), (int)size);
    savestream.close();
    m_binData.clear();

    uploader->FileSaved(File(str_path, hash), "");
    delete[] decryptedtext;
}


/** send message to client's info socket */
void WebsocketClient::sendMessageInfo(const ClientConnection& conn, EClientMessageType messageType, Json& arguments)
{
    websocketpp::lib::error_code ec;
    /* copy the argument values, and bundle the message type into the object */
    if (JsonWorker::FindStringVal(arguments, MESSAGE_FIELD).empty())
        JsonWorker::AddToJsonVal(arguments, MESSAGE_FIELD, std::to_string((int)messageType));
    else
        JsonWorker::ChangeVal(arguments, MESSAGE_FIELD, std::to_string((int)messageType));

    /* send the JSON data to the client (will happen on the networking thread's event loop) */
    m_wsClientInfo.send(conn, JsonWorker::Serialize(arguments), websocketpp::frame::opcode::text, ec);
}


/** client commands */
void WebsocketClient::command(WebsocketClient* client, int command_type, Json& options, const string& password) {

    JsonWorker::AddToJsonVal(options, "id", client->m_uuid);
    JsonWorker::AddToJsonVal(options, MESSAGE_FIELD,
                             std::to_string((int)EClientMessageType::NONE));

    string secret = "";
    if(password.size() > 0) {
        secret = client->generate_secret(password);
    }

    JsonWorker::AddToJsonVal(options, "secret", secret);

    auto type_command = (ECommandType)command_type;
    switch (type_command) {
        case ECommandType::SEND_FILE:
            client->commandSendFile(options, password);
            break;
        case ECommandType::RECEIVE_FILE:
            client->temporal_password[JsonWorker::FindStringVal(options, "hash")] = password;
            client->sendMessageInfo(client->m_connInfo, EClientMessageType::START_RECEIVE_FILE, options);
            break;
        case ECommandType::REMOVE_FILE:
            client->sendMessageInfo(client->m_connInfo, EClientMessageType::START_REMOVE_FILE, options);
            break;
        case ECommandType::RENAME_FILE:
            client->sendMessageInfo(client->m_connInfo, EClientMessageType::START_RENAME_FILE, options);
            break;
        case ECommandType::GET_FILE_LIST:
            client->sendMessageInfo(client->m_connInfo, EClientMessageType::START_GET_FILES_LIST, options);
            break;
    }
    client->message_ = JsonWorker::Serialize(options);
}

/** event to opening info socket */
void WebsocketClient::onOpenInfo(const ClientConnection& conn) {
    m_connInfo = conn;
}

/** event to failing info socket */
void WebsocketClient::onFailInfo(const ClientConnection& conn) {

}

/** event to messaging info socket */
void WebsocketClient::onMessageInfo(const ClientConnection& conn, const msg& msg) {

    Json newJson = JsonWorker::Deserialize(const_cast<string &>(msg->get_payload()));

    JsonWorker::print_json(newJson, "onMessageInfo");

    if (!newJson->HasParseError())
    {
        if (newJson->HasMember(MESSAGE_FIELD))
        {
            int command = 0;
            string str_comm = JsonWorker::FindStringVal(newJson, MESSAGE_FIELD);
            if (::isNumber(str_comm) && !str_comm.empty())
                command = stoi(str_comm);

            auto type_command = (EServerMessageType)command;

            switch (type_command) {
                case EServerMessageType::FILE_SENT:
                {
                    auto path = JsonWorker::FindStringVal(newJson, "path");
                    auto hash = JsonWorker::FindStringVal(newJson, "hash");
                    WLI << "file " << path << " was sent" << std::endl;
                    message_ = "file " + path + " was sent";
                    uploader->FileSent(File(path, hash), "");
                    break;
                }
                case EServerMessageType::FILE_REMOVED:
                {
                    auto path = JsonWorker::FindStringVal(newJson, "path");
                    auto hash = JsonWorker::FindStringVal(newJson, "hash");
                    WLI << "file " << path << " was removed" << std::endl;
                    message_ = "file " + path + " was removed";
                    uploader->FileRemoved(File(path, hash), "");
                    break;
                }
                case EServerMessageType::PERMISSION_ERROR:
                {
                    auto path = JsonWorker::FindStringVal(newJson, "path");
                    auto hash = JsonWorker::FindStringVal(newJson, "hash");
                    auto what = JsonWorker::FindStringVal(newJson, "what");
                    message_ = "PERMISSION_ERROR " + path + " : " + what;
                    WLI << message_;
                    uploader->FileError(File(path, hash), "");
                    temporal_password.erase(path);
                    break;
                }
                case EServerMessageType::FILE_RENAMED:
                {
                    auto old_path = JsonWorker::FindStringVal(newJson, "old_path");
                    auto new_path = JsonWorker::FindStringVal(newJson, "new_path");
                    auto hash = JsonWorker::FindStringVal(newJson, "hash");
                    WLI << "file " << old_path << " was renamed to " << new_path << std::endl;
                    message_ = "file " + old_path + " was renamed to " + new_path;
                    break;
                }
                case EServerMessageType::GET_FILES_LIST:
                {
                    WLI << "returned file list" << std::endl;
                    auto files = JsonWorker::FindStringVal(newJson, "files");
                    WLI << "files : " << files << std::endl;
                    uploader->UpdateUploadedFilesInfo(files);
                    message_ = "returned file list";
                    break;
                }
                case EServerMessageType::USER_REG_STEP_1:
                {
                    WLI << "user registration" << std::endl;
                    message_ = "user registration_step_1";
                    Json mergeJson = JsonWorker::CreateJsonObject();
                    JsonWorker::AddToJsonVal(mergeJson, MESSAGE_FIELD,
                                             std::to_string((int)EClientMessageType::MERGE_SOCKETS_STEP_1));
                    //m_aesKey = JsonWorker::FindStringVal(newJson, "aesKey");
                    m_uuid = JsonWorker::FindStringVal(newJson, "uuid");
                    JsonWorker::AddToJsonVal(mergeJson, "uuid", m_uuid);
                    JsonWorker::AddToJsonVal(mergeJson, "user_name", user_);

                    std::string priv, pub;

                    asymmetric::gen_pair(pub, priv);
                    private_password = priv;

                    std::string test = generateEncodeKey();

                    std::string re_test;

                    std::string encoded_test;

                    size_t ciphersize;

                    asymmetric::encrypt(test, encoded_test, pub, ciphersize);

                    asymmetric::decrypt(re_test, encoded_test, priv, ciphersize);

                    std::string ser_pub = "";

                    Serialize(pub.data(), pub.size(), ser_pub);


                    JsonWorker::AddToJsonVal(mergeJson, "user_password", sha256_string(pass_));
                    JsonWorker::AddToJsonVal(mergeJson, "public_password", ser_pub);

                    websocketpp::lib::error_code ec;

                    m_wsClientInfo.send(m_connInfo, JsonWorker::Serialize(mergeJson), websocketpp::frame::opcode::text, ec);
                    m_wsClientData.send(m_connData, JsonWorker::Serialize(mergeJson), websocketpp::frame::opcode::text, ec);
                    break;
                }
                case EServerMessageType::USER_REG_STEP_2:
                {
                    WLI << "user registration" << std::endl;
                    message_ = "user registration_step_2";
                    Json mergeJson = JsonWorker::CreateJsonObject();
                    JsonWorker::AddToJsonVal(mergeJson, MESSAGE_FIELD,
                                             std::to_string((int)EClientMessageType::MERGE_SOCKETS_STEP_2));
                    std::string serialize_encoded_key = JsonWorker::FindStringVal(newJson, "aesKey");
                    std::string str_ciphersize =  JsonWorker::FindStringVal(newJson, "ciphersize");

                    size_t ciphersize = std::stoull(str_ciphersize);

                    std::string encoded_key(serialize_encoded_key.size() / 2, '\0');

                    Deserialize(encoded_key.data(), encoded_key.size(), serialize_encoded_key);

                    asymmetric::decrypt(m_aesKey, encoded_key, private_password, ciphersize);

                    JsonWorker::AddToJsonVal(mergeJson, "uuid", m_uuid);

                    websocketpp::lib::error_code ec;

                    // todo(sedenkov): decide what to do
                    m_wsClientData.send(m_connData, JsonWorker::Serialize(mergeJson), websocketpp::frame::opcode::text, ec);

                    for (int i = 0; i < m_allDataConnections.size(); ++i) {
                        m_wsClientData.send(m_allDataConnections[i],
                                            JsonWorker::Serialize(mergeJson),
                                            websocketpp::frame::opcode::text, ec);
                    }
                    online = true;
                    break;
                }

                default:
                    break;

            }
        }
    }
}

/** event to closing info socket */
void WebsocketClient::onCloseInfo(const ClientConnection& conn) {

}

/** event to opening data socket */
void WebsocketClient::onOpenData(const ClientConnection& conn) {
    if (!m_isConnectionDataEstablished) {
        m_connData = conn;
        m_isConnectionDataEstablished = true;
        m_allDataConnections.push_back(conn);
    } else {
        m_allDataConnections.push_back(conn);
    }
}

/** event to failing data socket */
void WebsocketClient::onFailData(const ClientConnection& conn) {

}

/** event to messaging data socket */
void WebsocketClient::onMessageData(const ClientConnection& conn, const msg& msg) {

    // TODO(Sedenkov): rewrite
    Json msgJson = JsonWorker::Deserialize(const_cast<string &>(msg->get_payload()));

    JsonWorker::print_json(msgJson, "onMessageData");
    static size_t ___TTTT___ = 0;
    ___TTTT___++;
    if (msgJson->IsObject()) {
        ___TTTT___ = 0;
        if (!msgJson->HasParseError()) {

            if (msgJson->HasMember(MESSAGE_FIELD)) {
                int command = 0;
                string str_comm = JsonWorker::FindStringVal(msgJson, MESSAGE_FIELD);
                if (::isNumber(str_comm) && !str_comm.empty())
                    command = stoi(str_comm);

                auto type_command = (EServerMessageType) command;

                switch (type_command) {
                    case EServerMessageType::FILE_RECEIVED:
                    {
                        std::string str_path = JsonWorker::FindStringVal(msgJson, "path");
                        std::string hash = JsonWorker::FindStringVal(msgJson, "hash");
                        try{
                            saveFile(str_path, hash);
                            message_ = "file " + str_path + " was received";
                            uploader->FileResive(File(str_path, hash), "");
                        } catch (const std::exception & e) {
                            message_ = std::string("file save error : ") + e.what();
                            uploader->FileError(File(str_path, hash), "");
                        } catch (websocketpp::lib::error_code e) {
                            message_ = std::string("file save error : ") + e.message();
                            uploader->FileError(File(str_path, hash), "");
                        } catch (...) {
                            message_ = "file save other exception";
                            uploader->FileError(File(str_path, hash), "");
                        }
                        WLI << message_ << std::endl;


                        break;
                    }
                    default:
                        break;
                }
            }
        }
    }
    else
    {
        if(___TTTT___ >= 20)
        {
            ___TTTT___ = 0;
            WLI << "onMessageData No json message. m_binData.size() = " << m_binData.size() / 1024.0 / 1024.0 << "MB";
        }
        for (int i=0; i < msg->get_raw_payload().size(); i++) {
            m_binData.push_back((uint8_t) *(msg->get_raw_payload().data() + i));
        }
    }
}

/** event to closing data socket */
void WebsocketClient::onCloseData(const ClientConnection& conn) {

}
