#ifndef UPLOADER_H
#define UPLOADER_H
#include "settings.h"


#include "common/fileSys.h"

#include <any>
#include <filesystem>
#include <future>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <unordered_map>
#include <list>

void UpdateAndRender(int posx, int posy, int width, int height);

class WebsocketClient;

struct FunctionResult
{
    enum OperationType
    {
        HttpDownload,
        SftpDownload,

        HttpUpload,
        // EncryptFiles,
        SftpUpload,

        DecrypeFiles,
    } operation;
    bool              success;
    std::stringstream result;
};


class FileUploader
{
public:
    FileUploader();
    ~FileUploader();

    void RunClient();

    FunctionResult DownloadRequest(FileMap _list = FileMap(), const std::string& pass = "");
    FunctionResult UploadRequest(FileMap _list = FileMap(), const std::string& pass = "");
    FunctionResult DeleteRequest(FileMap _list = FileMap(), const std::string& pass = "");
    FunctionResult RenameRequest(std::pair<std::string, File> old, std::pair<std::string, File> file, const std::string& pass = "");

    void SetStyles();

    void ShowWindow(int posx, int posy, int width, int height);

    void Update();

    void UpdateUploadedFilesInfo(std::string data);

    typedef std::function<void (const File& file, std::string)> FileClaback;

    void FileSaved(const File& file, std::string meta);
    void FileRemoved(const File& file, std::string meta);
    void FileSent(const File& file, std::string meta);
    void FileUploaded(const File& file, std::string meta);
    void FileResive(const File& file, std::string meta);
    void FileError(const File& file, std::string meta);

    float _update_cur_file_sent_value;
    std::string _update_cur_file_sent_name;


    void SetLoadProgress(std::string archive_name, float);
    std::pair<const char *, float> GetLoadProgress();

private:

    std::unordered_map<void*, FileClaback*>
        _loaded_calbacks,
        _removed_calbacks,
        _sended_calbacks,
        _uploaded_callbacks,
        _resive_calbacks,
        _error_callbacks;


#define __GEN__CONTENT__(name)\
void name##_Head();\
void name##_Left();\
void name##_Right()

    __GEN__CONTENT__(None);
    __GEN__CONTENT__(Gov);
    __GEN__CONTENT__(Backup);
    __GEN__CONTENT__(Local);
    __GEN__CONTENT__(Archive);
    __GEN__CONTENT__(Restore);

#undef __GEN__CONTENT__


    // config

    Settings settings_;


    FileMap _archivedFilesInfo;
    FileMap _localeFilesInfo;

    WebsocketClient* client;

    int _update_file_list_status = 0;
};

#endif /* UPLOADER_H */
