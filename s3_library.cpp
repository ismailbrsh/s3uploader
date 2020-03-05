#include <cstring>
#include <atomic>
#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <libs3.h>
#include "plog/Log.h"

#ifdef __linux__
#   include <sys/stat.h>
#endif


#include "s3_library.h"

#ifdef _MSC_VER
#   define NOEXCEPT
#elif defined __GNUC__
#   define NOEXCEPT noexcept
#endif

struct BaseContext {
    explicit BaseContext(bool& error, void* child = nullptr) : error(error), child(child){
        static int last_id = 0;
        status = S3StatusOK;

        id = last_id++;
        //LOGD << "Create request:" << last_id;
    }
    virtual ~BaseContext() {};


    bool& error;
    int id;
    void* child;
    S3Status status;
};


S3Status responsePropertiesCallback(
        const S3ResponseProperties *properties,
        void *callbackData) {
    /*if(callbackData) {
        LOGD << "Response on request:" << ((BaseContext*)callbackData)->id;
    }*/
    return S3StatusOK;
}


static void responseCompleteCallback(
    S3Status status,
    const S3ErrorDetails *error,
    void *callbackData) {

    BaseContext* context = nullptr;
    if(callbackData) {
        context = (BaseContext *) callbackData;
        context->status = status;


    }

    if(status != S3StatusOK) {
       // LOGE << "Response error:" << (int)status << S3_get_status_name(status);
        if(context)
            context->error = true;
    }
}


S3ResponseHandler responseHandler = {
        &responsePropertiesCallback,
        &responseCompleteCallback
};


struct MyFileInfo {
    std::string url;
    uint64_t size;
    bool is_dir;
};
    struct TreeItem {
        MyFileInfo info;
        TreeItem *parent;
        std::vector<TreeItem> child;
    };


std::vector<std::string> split(const std::string& s, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}

std::string removePostfix(std::string file){
    //if(file.size() < 5)
        return file;

    if(file[file.size() - 2] == 'v' && file[file.size() - 3] == 'k' && file[file.size() -4] == 'm') {
        size_t under_pos = file.rfind('_');


        if (under_pos != std::string::npos) {


            size_t dot_pos = file.rfind('.');
            if (dot_pos != std::string::npos) {
                if (dot_pos - under_pos < 7) {
                    file = file.erase(under_pos, dot_pos - under_pos);
                }
            }
        }
    }

    return file;
}

int getIndex(std::vector<TreeItem>& items, const std::string& name){
    for(size_t i = 0; i < items.size(); i++){
        if(items[i].info.url == name)
            return i;
    }

    return -1;
}

std::vector<MyFileInfo> iterateDir(const std::shared_ptr<TreeItem>& tree,  const std::string& dir, const std::string& bucket){
    std::vector<MyFileInfo> result;
    TreeItem* current = tree.get();
    std::vector<std::string> names = split(dir, '/');

    std::string path = "/" + bucket;

    for(const auto& name:names){
        if(name.empty())
            continue;
        bool found = false;
        for(auto& item: current->child){
            if(item.info.url == name){
                found = true;
                path += "/" + name;
                current = &item;
                break;
            }
        }

        if(!found) {
            return result;
        }
    }

    for(const auto& item: current->child){
        MyFileInfo info;
        info.url = path + "/" + item.info.url;
        info.is_dir = !item.child.empty();
        info.size = !item.child.empty() ? 0 : item.info.size;


        result.push_back(info);
    }
    return result;
}


std::string url2key(std::string url){
    if(url[url.size() - 1] == '/')
        url = url.substr(0, url.size() - 1);
    if(url.substr(0, 5) == "/opt/"){
        size_t at_pos = url.find('@');
        if(at_pos == std::string::npos){
            return url.substr(url.find('/', 1) + 1);
        }

        size_t pos1 = url.find('/', at_pos + 1);
        size_t pos2 = url.find('/', pos1 + 1);

        return url.substr(pos2 + 1);
    }

    if(url[0] == '/')
        return url.substr(url.find('/', 1) + 1);

    return url;
}


std::string key2url(const std::string& key, const std::string& bucket, uint64_t max_size){
    if(key[0] == '/')
        return '/' + bucket + key;


    return '/' + bucket + (max_size > 0 ? std::to_string(max_size) : std::string("/")) + key;
}


struct ListServiceContext {
    ListServiceContext(const std::string& bucket_name, bool& found) : bucket_name(bucket_name), found(found){}

    const std::string& bucket_name;
    bool& found;
};

struct IterateFilesContext {
    IterateFilesContext(std::vector<MyFileInfo>& files, std::string& marker) : files(files), marker(marker) {}

    std::vector<MyFileInfo>& files;
    std::string& marker;
};

static S3Status listServiceCallback(
        const char *ownerId,
        const char *ownerDisplayName,
        const char *bucketName,
        int64_t creationDate, void *callbackData) {
    auto base_context = (BaseContext*) callbackData;


    if(!base_context)
        return S3StatusAbortedByCallback;


    auto context = (ListServiceContext*)base_context->child;


    if(!context)
        return S3StatusAbortedByCallback;


    if(context->bucket_name == bucketName) {
        context->found = true;
        LOGD << "bucket found:" << bucketName;
    }


    return S3StatusOK;
}

static S3Status listBucketCallback(
        int isTruncated,
        const char *nextMarker,
        int contentsCount,
        const S3ListBucketContent *contents,
        int commonPrefixesCount,
        const char **commonPrefixes,
        void *callbackData)
{
    auto* base_context = (BaseContext*)callbackData;

    if(!base_context)
        return S3StatusAbortedByCallback;
    auto* context = (IterateFilesContext*)base_context->child;

    if(!context)        
        return S3StatusOK;
    for (int i = 0; i < contentsCount; i++) {
        const S3ListBucketContent *content = &(contents[i]);


        MyFileInfo file_info;
        file_info.size = content->size;
        file_info.url  = content->key;
        file_info.is_dir = false;
        //LOGD << content->key << ":" <<content->size;
        context->files.push_back(file_info);
    }

    for(int i = 0; i < commonPrefixesCount; i++){
        MyFileInfo file_info;
        file_info.size = 0;
        file_info.url  = commonPrefixes[i];
        file_info.is_dir = true;
        //LOGD << file_info.url;
        context->files.push_back(file_info);
    }
    //LOGD << contentsCount;
    if(isTruncated){
        context->marker = nextMarker;
 //       LOGD << "Truncated:" << nextMarker;
    } else {
        context->marker = "";
    }

    return S3StatusOK;
}


typedef struct put_object_callback_data
{
    FILE *infile;
    uint64_t contentLength;
} put_object_callback_data;


struct PutObject {
    explicit PutObject(put_object_callback_data& data) : data(data){}
    virtual ~PutObject() {};


    put_object_callback_data data;
};


static int putObjectDataCallback(int bufferSize, char *buffer, void *callbackData)
{
    if(!callbackData)
        return -1;
    BaseContext* base_context = (BaseContext*)callbackData;
    if(!base_context)
        return S3StatusAbortedByCallback;


    PutObject* context = (PutObject*)base_context->child;
    put_object_callback_data *data = &context->data;


    int ret = 0;


    if (data->contentLength) {
        int toRead = ((data->contentLength > (unsigned) bufferSize) ? (unsigned) bufferSize : data->contentLength);
        ret = fread(buffer, 1, toRead, data->infile);
    }
    data->contentLength -= ret;
    return ret;
}






namespace nx_spl
{
    struct GetObject :public BaseContext {
        explicit GetObject(FILE* file, bool& error) : BaseContext(error), file(file){}
        virtual ~GetObject() {};


        FILE* file;
    };
    static S3Status getObjectDataCallback(int bufferSize, const char *buffer, void *callbackData)
    {
        GetObject* context = (GetObject*)callbackData;


        FILE *outfile = context->file;
        size_t wrote = fwrite(buffer, 1, bufferSize, outfile);


//        LOGD << "callback write:" << wrote;
        return ((wrote < (size_t) bufferSize) ? S3StatusAbortedByCallback : S3StatusOK);
    }


    static S3GetObjectHandler getObjectHandler =
            {
                    responseHandler,
                    &getObjectDataCallback
            };

    static void
    collectFiles(const std::string &access_key, const std::string &secret_key, const std::string &bucket_name,
                 const std::string &host, std::vector<MyFileInfo> &files, const char *prefix,
                 const char *delimiter) {
        S3BucketContext bucketContext;

        bucketContext.accessKeyId       = access_key.c_str();
        bucketContext.secretAccessKey   = secret_key.c_str();
        bucketContext.authRegion        = nullptr;
        bucketContext.bucketName        = bucket_name.c_str();
        bucketContext.hostName          = host.c_str();
        bucketContext.protocol          = S3ProtocolHTTPS;
        bucketContext.uriStyle          = S3UriStylePath;
        bucketContext.securityToken     = nullptr;


        S3ListBucketHandler listBucketHandler =
                {
                        responseHandler,
                        &listBucketCallback
                };
        std::string marker;
        do {
            bool error = false;

            IterateFilesContext context(files, marker);
            BaseContext base_context(error, &context);

            S3_list_bucket(&bucketContext, prefix, marker.empty() ? nullptr : marker.c_str(), delimiter, 1000000, nullptr, 10000, &listBucketHandler,
                           &base_context);

            if (error) {
                LOGE << "Couldn't list bucket";
                break;
            }

            if(marker.empty())
                break;
        }while(true);
    }
    namespace aux
    {
// Scoped file remover
        class FileRemover
        {
        public:
            FileRemover(const std::string& fname) :
                    m_fname(fname)
            {}

            ~FileRemover()
            {
                std::remove(m_fname.c_str());
            }
        private:
            std::string m_fname;
        };

// Generates pseudo-random string for use as unique file name
// Strictly speaking, the uniquness is not guaranteed, so delete files as soon as possible.
        std::string getRandomFileName()
        {
            std::stringstream randomStringStream;
            randomStringStream << std::hex << std::rand() << std::rand();


            return randomStringStream.str();
        }


// Cut dir name from file name and return both.
        void dirFromUri(
                const std::string   &uri, // In
                std::string         *dir, // Out. directory name
                std::string         *file // Out. file name
        )
        {
            std::string::size_type pos;
            if ((pos = uri.rfind('/')) == std::string::npos)
            {   // Here we think that '/' is the only path separator.
                // Maybe generally it is not very reliable.
                *dir = ".";
                *file = uri;
            }
            else
            {
                dir->assign(uri.begin(), uri.begin() + pos);
                if (pos < uri.size() - 1) // if file name is not empty
                    file->assign(uri.begin() + pos + 1, uri.end());
            }
        }




// Checks if remote file exists. Bases on MLSD command response parsing.
/*        bool remoteFileExists(const std::string& access_key, const std::string& secret_key, const std::string& host, const std::string& bucket_name, const std::string& uri)
        {
            LOGD << "Check if remote exists";
            std::string file = url2key(uri);
            file = removePostfix(file);


            std::vector<MyFileInfo> files;
            collectFiles(access_key, secret_key, bucket_name, host, files, nullptr);




            std::string dir;


            if(files.empty()) {
                return 0;
            }




            auto result = std::find_if(files.begin(), files.end(), [file](const MyFileInfo& info){
                return info.url == file;
            });
            return result != files.end();
        }
*/
// set error code to initial state (NoError generally if storage is available)
        error::code_t checkECode(
                int            *checked,                // ecode to set
                const int       avail,                  // pass result of getAvail here
                error::code_t   toSet = error::NoError  // default ecode
        )
        {
            if (checked)
                *checked = error::NoError;


            if (!avail)
            {
                if (checked)
                    *checked = error::StorageUnavailable;
                return error::StorageUnavailable;
            }
            else if (checked)
                *checked = toSet;


            return (error::code_t)*checked;
        }


// get local file size by it's name
        long long getFileSize(const char *fname)
        {
#ifdef _WIN32


            HANDLE hFile = CreateFileA(
        fname,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (hFile == INVALID_HANDLE_VALUE)
        return -1;


    LARGE_INTEGER s;
    if (!GetFileSizeEx(hFile, &s))
    {
        CloseHandle(hFile);
        return -1;
    }
    CloseHandle(hFile);
    return s.QuadPart;


#elif defined __linux__


            struct stat st;
            if (stat(fname, &st) == -1)
                return -1;
            return st.st_size;
#endif
        }




    } // namespace






// FileInfo Iterator
S3FileInfoIterator::S3FileInfoIterator(
    FileListType      &&fileList
)
    :
        m_fileList(std::move(fileList)),
        m_curFile(m_fileList.cbegin())
{
        //LOGD << "File iterator has:" << m_fileList.size();
}


S3FileInfoIterator::~S3FileInfoIterator()
{}


FileInfo* STORAGE_METHOD_CALL S3FileInfoIterator::next(int* ecode) const
{
    if (ecode)
        *ecode = nx_spl::error::NoError;

    if (m_curFile != m_fileList.cend())
    {
        FileInfo info;
        info.url = m_curFile->url.c_str();
        info.type = m_curFile->is_dir ? isDir : isFile;
        info.size = m_curFile->size;

        //LOGD << "Name:" << m_curFile->url << ", " << (m_curFile->is_dir ? "dir" : "file") << " , size:" << m_curFile->size;

        m_info = info;
        ++m_curFile;

        return &m_info;
    }

    return nullptr;
}

void* S3FileInfoIterator::queryInterface(const nxpl::NX_GUID& interfaceID)
{
    if (std::memcmp(&interfaceID,
                    &IID_FileInfoIterator,
                    sizeof(nxpl::NX_GUID)) == 0)
    {
        addRef();
        return static_cast<nx_spl::FileInfoIterator*>(this);
    }
    else if (std::memcmp(&interfaceID,
                            &nxpl::IID_PluginInterface,
                            sizeof(nxpl::IID_PluginInterface)) == 0)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

unsigned int S3FileInfoIterator::addRef()
{
    return p_addRef();
}

unsigned int S3FileInfoIterator::releaseRef()
{
    return p_releaseRef();
}




// S3 Storage
//s3://login:password@host/bucket
S3Storage::S3Storage(const std::string& url)
    : m_available(false), m_max_size(0)
{
    LOGD << "Create storage for url:" << url;


    size_t login_password_sep_pos = url.find(':', 5);
    size_t at_pos = url.find('@');


    size_t end_host_pos = url.rfind('/');




    if(end_host_pos == std::string::npos || url.length() < 6 || login_password_sep_pos == std::string::npos || at_pos == std::string::npos){
        LOGE << "Couldn't get host,%s\n", url;


        m_available = false;
        return;
    }


    m_access_key = url.substr(5, login_password_sep_pos - 5);
    m_secret_key = url.substr(login_password_sep_pos + 1, at_pos - login_password_sep_pos - 1);






    m_host = url.substr(at_pos + 1, end_host_pos - at_pos - 1);
    m_bucket_name = url.substr(end_host_pos + 1);


    size_t size_delimeter_pos = m_bucket_name.find('@');


    LOGD << m_bucket_name << "," << size_delimeter_pos;
    if(size_delimeter_pos != std::string::npos){
        m_max_size = std::stol(m_bucket_name.substr(size_delimeter_pos + 1));
        m_bucket_name = m_bucket_name.substr(0, size_delimeter_pos);


        LOGD << m_bucket_name;
    }


    if(m_host.empty() || m_bucket_name.empty()){
        LOGE <<"Bucket or host is empty,%s\n";


        m_available = false;
        return;
    }


    auto res = S3_initialize("s3", S3_INIT_ALL, m_host.c_str());


    if(res != S3StatusOK) {
        LOGE <<  "Error when initialize S1", (int)res;


        m_available = false;
        return;
    }
    else{
        LOGD << "Initialize S3:OK";
    }


    bool found = false;
    bool error = false;


    ListServiceContext context(m_bucket_name, found);


    BaseContext base_context(error, &context);
    S3ListServiceHandler listServiceHandler = {
            responseHandler,
            &listServiceCallback
    };




    S3_list_service(S3ProtocolHTTPS, m_access_key.c_str(), m_secret_key.c_str(), nullptr, m_host.c_str(),
                    nullptr, nullptr, 10000, &listServiceHandler, &base_context);


    if(error){
        LOGE << "Couldn't list buckets" << url.c_str();


        m_available = false;
        return;
    }


    if(!found){
        S3_create_bucket(S3ProtocolHTTPS, m_access_key.c_str(), m_secret_key.c_str(), NULL, m_host.c_str(), m_bucket_name.c_str(), NULL, S3CannedAclPrivate, NULL, NULL, 10000, &responseHandler, &context);
        LOGD << "Create bucket";
    } else {
        LOGD << "Bucket exists";
    }


    if(error){
        LOGE << "Couldn't create bucket,%s\n";


        m_available = false;
        return;
    }


    m_available = true;


    terminate_thread = false;
    t = std::make_shared<std::thread>([this]{
        LOGD << "=====================:" << terminate_thread;
       int counter = 2 * 3600 * 2;
        while(!terminate_thread){
           usleep(500000);
           if(counter++ > 2 * 3600 * 2){
               counter = 0;
               std::vector<MyFileInfo> files;
               collectFiles(m_access_key, m_secret_key, m_bucket_name, m_host, files, nullptr, nullptr);


               uint64_t used_space = 0;


               for (const auto &f : files) {
                   used_space += f.size;
               }
                LOGD << "Used space:" << used_space;
               std::string str = aux::getRandomFileName();
               time_t time_now = time(nullptr);
               FILE* f = fopen(str.c_str(), "w");
               fprintf(f, "%d\n%llu\n", time_now, used_space);
               fclose(f);


               f = fopen(str.c_str(), "r");


               put_object_callback_data data;


               struct stat statbuf;
               if (stat(str.c_str(), &statbuf) == -1) {
                   LOGE << "Culdn't get file size:" << str;
                   return;
               }


               int contentLength = statbuf.st_size;
               data.contentLength = contentLength;


               if (!(data.infile = f)) {
                   LOGE << "Couldn't open:" << str;
                   return;
               }


               bool error = false;


               PutObject context(data);
               BaseContext base_context(error, &context);
               S3PutObjectHandler putObjectHandler =
                       {
                               responseHandler,
                               &putObjectDataCallback
                       };


               S3BucketContext bucketContext;


               bucketContext.accessKeyId       = m_access_key.c_str();
               bucketContext.secretAccessKey   = m_secret_key.c_str();
               bucketContext.authRegion        = nullptr;
               bucketContext.bucketName        = m_bucket_name.c_str();
               bucketContext.hostName          = m_host.c_str();
               bucketContext.protocol          = S3ProtocolHTTPS;
               bucketContext.uriStyle          = S3UriStylePath;
               bucketContext.securityToken     = nullptr;


               LOGD << "Put object:" << ".size" << ", " << contentLength;


               S3_put_object(&bucketContext, ".size", contentLength, NULL, NULL, 0, &putObjectHandler, &base_context);
               fclose(f);
               remove(str.c_str());
           }
       }
    });


    LOGD << "===++==";
}


S3Storage::~S3Storage()
{
        LOGD << "Destroy storage";
        terminate_thread = true;
        t->join();
    S3_deinitialize();
}


int STORAGE_METHOD_CALL S3Storage::isAvailable() const
{   // If PASV failed it means that either there are some network problems
    // or just we've been idle for too long and server has closed control session.
    // In the latter case we can try to reestablish connection.


    std::lock_guard<std::mutex> lock(m_mutex);
    //LOGD << "IsAvailable:" << m_available;
    return test_bucket();
}


uint64_t S3Storage::getUsedSpace() const {
      //  LOGD << "Get used space";
    bool error = false;
    BaseContext base_context(error);
    S3BucketContext bucketContext;

    bucketContext.accessKeyId       = m_access_key.c_str();
    bucketContext.secretAccessKey   = m_secret_key.c_str();
    bucketContext.authRegion        = nullptr;
    bucketContext.bucketName        = m_bucket_name.c_str();
    bucketContext.hostName          = m_host.c_str();
    bucketContext.protocol          = S3ProtocolHTTPS;
    bucketContext.uriStyle          = S3UriStylePath;
    bucketContext.securityToken     = nullptr;

    std::string temp_file = aux::getRandomFileName();


    FILE *f = fopen(temp_file.c_str(), "wb");
    if (f == NULL) {
        //throw std::runtime_error("Couldn't open file for writing");
        return 0;
    }

    error = false;
    GetObject context(f, error);
    //LOGD << "Get object";

    S3_get_object(&bucketContext, ".size", NULL, 0, 0, NULL, 120000, &getObjectHandler, &context);
    fclose(f);

    if(error)
        return 0;

    f = fopen(temp_file.c_str(), "r");
    char time_buf[32];
    char size_buf[32];

    fscanf(f, "%s%s", time_buf, size_buf);

    fclose(f);
    remove(temp_file.c_str());
    char* end;
    return std::strtoull(size_buf, &end, 10);
}


uint64_t STORAGE_METHOD_CALL S3Storage::getFreeSpace(int* ecode) const
{   // In general there is no reliable way to determine FTP server free disk space
    //if (ecode)
    //    *ecode = error::SpaceInfoNotAvailable;
    //return 1024LL * 1024LL * 1024 * 100;//unknown_size;
    std::lock_guard<std::mutex> lock(m_mutex);


    if (ecode)
        *ecode = error::NoError;
    if(m_max_size == 0)
        return 100LL * 1024 * 1024 * 1024;
    uint64_t used_space = getUsedSpace();
//    LOGD << "Get free space";



    if(m_max_size * (1024LL * 1024LL * 1024LL) < used_space) {
       // LOGD << m_max_size * (1024LL * 1024LL * 1024LL) << used_space;
        return 0;
    }
  //  LOGD << "*********************   " << m_max_size * (1024LL * 1024LL * 1024LL) - used_space;
    return m_max_size * 1024LL * 1024LL * 1024LL - used_space;

    //return 1000LL * 1000 * 1000 * 100; // for tests
}


uint64_t STORAGE_METHOD_CALL S3Storage::getTotalSpace(int* ecode) const
{   // In general there is no reliable way to determine FTP server total disk space
    //if (ecode)
    //    *ecode = error::SpaceInfoNotAvailable;
    //return unknown_size;
    if (ecode)
        *ecode = error::NoError;
    if(m_max_size)
        return m_max_size * (1024 * 1024 * 1024);
    return 1000LL * 1000 * 1000 * 100; // for tests
}


int STORAGE_METHOD_CALL S3Storage::getCapabilities() const
{
//        LOGD << "get cap";
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!getAvail())
        return 0;

//    LOGD << "get cap";

    std::string fileName(aux::getRandomFileName());
    aux::FileRemover fr(fileName);


    bool error = false;
    BaseContext base_context(error);
    S3BucketContext bucketContext;

    bucketContext.accessKeyId       = m_access_key.c_str();
    bucketContext.secretAccessKey   = m_secret_key.c_str();
    bucketContext.authRegion        = nullptr;
    bucketContext.bucketName        = m_bucket_name.c_str();
    bucketContext.hostName          = m_host.c_str();
    bucketContext.protocol          = S3ProtocolHTTPS;
    bucketContext.uriStyle          = S3UriStylePath;
    bucketContext.securityToken     = nullptr;

    // list
    int ret = 0;

    S3ListBucketHandler listBucketHandler =
            {
                    responseHandler,
                    &listBucketCallback
            };

    std::vector<MyFileInfo> files;
    std::string marker;
    IterateFilesContext context(files, marker);

    S3_list_bucket(&bucketContext, nullptr, nullptr, "/", 10, nullptr, 10000, &listBucketHandler,&base_context);
//    LOGD << "get cap, " << base_context.error << "," << S3_get_status_name(base_context.status);
    if(!error)
        ret |= cap::ListFile;
    else
        return ret;
  //  LOGD << "get cap";
    FILE* ofs = fopen(fileName.c_str(), "wb");
    // write file
    if (ofs)
    {
        fwrite("1", 1, 2, ofs);
        fclose(ofs);

        put_object_callback_data data;


        struct stat statbuf;
        if (stat(fileName.c_str(), &statbuf) == -1) {
            LOGE << "Culdn't get file size:" << fileName;
            return ret;
        }

        int contentLength = statbuf.st_size;
        data.contentLength = contentLength;

        if (!(data.infile = fopen(fileName.c_str(), "r"))) {
            LOGE << "Couldn't open:" << fileName;
            return ret;
        }

        bool error = false;

        PutObject context(data);
        BaseContext base_context(error, &context);
        S3PutObjectHandler putObjectHandler =
                {
                        responseHandler,
                        &putObjectDataCallback
                };


        S3_put_object(&bucketContext, fileName.c_str(), contentLength, NULL, NULL, 0, &putObjectHandler, &base_context);
    //    LOGD << "Put complete";
        fclose(data.infile);

        if(!error)
        {
            ret |= cap::WriteFile;
        }
    }
    else {
      //  LOGD << "get cap";
        return ret;
    }
    error = false;

    S3_head_object(&bucketContext, fileName.c_str(), nullptr, 0, &responseHandler, &base_context);
    //LOGD << "get cap";
    // read file
    if(!error) {
      //  LOGD << "get cap";
        ret |= cap::ReadFile;
    }

    error = false;

    S3_delete_object(&bucketContext,
                     fileName.c_str(),
                     nullptr,
                     0,
                     &responseHandler, &context);


    // remove file
    if (!error)
        ret |= cap::RemoveFile;

    //LOGD << "get cap:" << ret;
    return ret;
}


void STORAGE_METHOD_CALL S3Storage::removeFile(
    const char  *url,
    int         *ecode
)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(aux::checkECode(ecode, getAvail()) != nx_spl::error::NoError)
        return;


    LOGD << "**************************************   Remove file:" << url << ", " << url2key(url);


    if(url2key(url).find("nxdb") != std::string::npos)
        return;


    S3BucketContext bucketContext;
    bucketContext.accessKeyId       = m_access_key.c_str();
    bucketContext.secretAccessKey   = m_secret_key.c_str();
    bucketContext.authRegion        = nullptr;
    bucketContext.bucketName        = m_bucket_name.c_str();
    bucketContext.hostName          = m_host.c_str();
    bucketContext.protocol          = S3ProtocolHTTPS;
    bucketContext.uriStyle          = S3UriStylePath;
    bucketContext.securityToken     = nullptr;


    bool error = false;
    BaseContext context(error);


    S3_delete_object(&bucketContext,
                   url2key(url).c_str(),
                   nullptr,
                   0,
                   &responseHandler, &context);


}


void STORAGE_METHOD_CALL S3Storage::removeDir(
    const char  *url,
    int         *ecode
)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(aux::checkECode(ecode, getAvail()) != nx_spl::error::NoError)
        return;


    LOGD << "**************************************   remove dir:" << url;


}


void STORAGE_METHOD_CALL S3Storage::renameFile(
    const char*     oldUrl,
    const char*     newUrl,
    int*            ecode
)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(aux::checkECode(ecode, getAvail()) != nx_spl::error::NoError)
        return;


    LOGD << "**************************************  Rename file:" << oldUrl << ", " << newUrl;


    S3BucketContext bucketContext;
    bucketContext.accessKeyId       = m_access_key.c_str();
    bucketContext.secretAccessKey   = m_secret_key.c_str();
    bucketContext.authRegion        = nullptr;
    bucketContext.bucketName        = m_bucket_name.c_str();
    bucketContext.hostName          = m_host.c_str();
    bucketContext.protocol          = S3ProtocolHTTPS;
    bucketContext.uriStyle          = S3UriStylePath;
    bucketContext.securityToken     = nullptr;


    bool error = false;
    BaseContext context(error);


    S3_copy_object(&bucketContext, url2key(oldUrl).c_str(),
                        m_bucket_name.c_str(),
                        url2key(newUrl).c_str(),
                        nullptr,
                        0, 0,
                        nullptr, nullptr,
                        0,
                        &responseHandler, &context);


    S3_delete_object(&bucketContext, url2key(oldUrl).c_str(), nullptr, 20000, &responseHandler, &context);
    /*if (m_impl->Rename(oldUrl, newUrl) == 0 && ecode)
        *ecode = error::UnknownError;*/
}

FileInfoIterator* STORAGE_METHOD_CALL S3Storage::getFileIterator(
    const char*     dirUrl,
    int*            ecode
) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(aux::checkECode(ecode, getAvail()) != nx_spl::error::NoError)
        return nullptr;

    std::string key_dir = url2key(dirUrl);
//    LOGD << "Iterate dir:" << dirUrl << ", " << key_dir;

    if(key_dir[key_dir.size() - 1] != '/')
        key_dir += '/';
 //   LOGD << key_dir;

    std::vector<MyFileInfo> files;
    collectFiles(m_access_key, m_secret_key, m_bucket_name, m_host, files, key_dir.c_str(), "/");

    return new S3FileInfoIterator(std::move(files));
}


int STORAGE_METHOD_CALL S3Storage::fileExists(
    const char  *url,
    int         *ecode
) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(aux::checkECode(ecode, getAvail()) != nx_spl::error::NoError)
        return 0;

    std::string file = url;

    size_t pos = file.find('/', 1);
    if (pos != std::string::npos)
        file = file.substr(pos + 1);

    bool fileExists = false;
    bool error = false;
    BaseContext base_context(error);
    S3BucketContext bucketContext;

    bucketContext.accessKeyId       = m_access_key.c_str();
    bucketContext.secretAccessKey   = m_secret_key.c_str();
    bucketContext.authRegion        = nullptr;
    bucketContext.bucketName        = m_bucket_name.c_str();
    bucketContext.hostName          = m_host.c_str();
    bucketContext.protocol          = S3ProtocolHTTPS;
    bucketContext.uriStyle          = S3UriStylePath;
    bucketContext.securityToken     = nullptr;


    std::string file_name = removePostfix(url2key(url));
    S3_head_object(&bucketContext, file_name.c_str(), nullptr, 0, &responseHandler, &base_context);
    fileExists = !error;

//    LOGD << "File exists:" << url << ", " << file_name << fileExists;

    return fileExists;
}


int STORAGE_METHOD_CALL S3Storage::dirExists(
    const char  *url,
    int         *ecode
) const
{
    return 0;
}


uint64_t STORAGE_METHOD_CALL S3Storage::fileSize(
    const char*     url,
    int*            ecode
) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(aux::checkECode(ecode, getAvail()) != nx_spl::error::NoError)
        return 0;
    //LOGD << "Get file size" << url << ", " << url2key(url);
    std::vector<MyFileInfo> files;
    collectFiles(m_access_key, m_secret_key, m_bucket_name, m_host, files, url2key(url).c_str(), nullptr);

    if(files.empty())
        return 0;

    auto result = std::find_if(files.begin(), files.end(), [url](const MyFileInfo& info){
        return info.url == url;
    });

    if(result == files.end()) {
        LOGD << "Couldn't get file size";
        return unknown_size;
    }
    LOGD << "Size:" << result->size;
    return result->size;
}

IODevice* STORAGE_METHOD_CALL S3Storage::open(
    const char*     uri,
    int             flags,
    int*            ecode
) const
{
//        LOGD << "**************************************  IoDevice open:" << uri << " mode:" << flags;
    *ecode = error::NoError;
    std::string uri_safe = uri;
    IODevice *ret = nullptr;
    removePostfix(uri_safe);
    if (!getAvail())
    {
        LOGE << "Storage unavailable";
        *ecode = error::StorageUnavailable;
        return nullptr;
    }


    try {
        ret = new S3IODevice(
                uri_safe.c_str(), flags, "", m_access_key, m_secret_key, m_host, m_bucket_name
        );
    }catch (const std::exception& e){
        LOGE << e.what();
        return nullptr;
    }
    return ret;
}


void* S3Storage::queryInterface(const nxpl::NX_GUID& interfaceID)
{
    if (std::memcmp(&interfaceID,
                    &IID_Storage,
                    sizeof(nxpl::NX_GUID)) == 0)
    {
        addRef();
        return static_cast<nx_spl::Storage*>(this);
    }
    else if (std::memcmp(&interfaceID,
                            &nxpl::IID_PluginInterface,
                            sizeof(nxpl::IID_PluginInterface)) == 0)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}


unsigned int S3Storage::addRef()
{
    return p_addRef();
}


unsigned int S3Storage::releaseRef()
{
    return p_releaseRef();
}


// test bucket ---------------------------------------------------------------
    int S3Storage::should_retry() const
    {
        if (retriesG--) {
            // Sleep before next retry; start out with a 1 second sleep
            static int retrySleepInterval = 1;
            sleep(retrySleepInterval);
            // Next sleep 1 second longer
            retrySleepInterval++;
            return 1;
        }

        return 0;
    }

bool S3Storage::test_bucket() const
    {
        S3ResponseHandler responseHandler =
                {
                        &responsePropertiesCallback, &responseCompleteCallback
                };

        char locationConstraint[64];

        static int statusG = 0;
        retriesG = 5;
        bool error = false;
        BaseContext context(error);
        do {
            error = false;
            S3_test_bucket(S3ProtocolHTTPS, S3UriStylePath, m_access_key.c_str(), m_secret_key.c_str(), nullptr,
                           m_host.c_str(), m_bucket_name.c_str(), nullptr, sizeof(locationConstraint),
                           locationConstraint, nullptr, 20000, &responseHandler, &context);
        } while (S3_status_is_retryable(context.status) && should_retry());


        LOGD << "Test bucket:" << !error;
        return !error;
    }


// S3StorageFactory
S3StorageFactory::S3StorageFactory()
{
    std::srand(time(0));
}


void* S3StorageFactory::queryInterface(const nxpl::NX_GUID& interfaceID)
{
    if (std::memcmp(&interfaceID,
                    &IID_StorageFactory,
                    sizeof(nxpl::NX_GUID)) == 0)
    {


        addRef();
        return static_cast<S3StorageFactory*>(this);
    }
    else if (std::memcmp(&interfaceID,
                            &nxpl::IID_PluginInterface,
                            sizeof(nxpl::IID_PluginInterface)) == 0)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}


unsigned int S3StorageFactory::addRef()
{
    return p_addRef();
}


unsigned int S3StorageFactory::releaseRef()
{
    return p_releaseRef();
}


const char** STORAGE_METHOD_CALL S3StorageFactory::findAvailable() const
{
    assert(false);
    return nullptr;
}


Storage* STORAGE_METHOD_CALL S3StorageFactory::createStorage(
    const char* url,
    int*        ecode
)
{
    Storage* ret = nullptr;
    *ecode = error::NoError;
    ret = new S3Storage(url);
    return ret;
}


const char* STORAGE_METHOD_CALL S3StorageFactory::storageType() const
{
    return "s3";
}


#define ERROR_LIST(APPLY) \
APPLY(nx_spl::error::EndOfFile) \
APPLY(nx_spl::error::NoError) \
APPLY(nx_spl::error::NotEnoughSpace) \
APPLY(nx_spl::error::ReadNotSupported) \
APPLY(nx_spl::error::SpaceInfoNotAvailable) \
APPLY(nx_spl::error::StorageUnavailable) \
APPLY(nx_spl::error::UnknownError) \
APPLY(nx_spl::error::UrlNotExists) \
APPLY(nx_spl::error::WriteNotSupported)


#define STR_ERROR(ecode) case ecode: return #ecode;


const char* S3StorageFactory::lastErrorMessage(int ecode) const
{
    switch(ecode)
    {
        ERROR_LIST(STR_ERROR);
    }
    return "";
}
#undef PRINT_ERROR
#undef ERROR_LIST




// Ftp IO Device
S3IODevice::S3IODevice(
        const char         *uri,
        int                 mode,
        const std::string  &storageUrl,
        const std::string  &access_key,
        const std::string  &secret_key,
        const std::string  &host,
        const std::string  &bucket_name
)
    : m_mode(mode),
        m_pos(0),
        m_uri(uri),
        m_altered(false),
        m_localsize(0),
        m_access_key(access_key),
        m_secret_key(secret_key),
        m_host(host),
        m_bucket_name(bucket_name)
{
    //  If file opened for read-only and no such file uri in storage throw BadUrl
    //  If file opened for write and no such file uri in stor - create it.
    std::lock_guard<std::mutex> lock(m_mutex);


    size_t pos = m_uri.find('/', 1);
    if(pos != -1)
        m_uri = m_uri.substr(pos + 1);


    m_uri = url2key(m_uri);


    std::string remoteDir, remoteFile;
    aux::dirFromUri(uri, &remoteDir, &remoteFile);
    m_localfile = aux::getRandomFileName() + "_" + remoteFile;
    bool fileExists = false;


    bool error = false;
    BaseContext base_context(error);


    S3BucketContext bucketContext;


    bucketContext.accessKeyId       = access_key.c_str();
    bucketContext.secretAccessKey   = secret_key.c_str();
    bucketContext.authRegion        = nullptr;
    bucketContext.bucketName        = bucket_name.c_str();
    bucketContext.hostName          = host.c_str();
    bucketContext.protocol          = S3ProtocolHTTPS;
    bucketContext.uriStyle          = S3UriStylePath;
    bucketContext.securityToken     = nullptr;


    S3_head_object(&bucketContext, m_uri.c_str(), nullptr, 0, &responseHandler, &base_context);


    fileExists = !error;


//    LOGD << "Head complete, mode:" << mode << ", error:" << error;


    if (mode & io::WriteOnly)
    {
//        LOGD << "Open write:" << mode;
        if (!fileExists)
        {
            FILE *f = fopen(m_localfile.c_str(), "wb");
            if (f == NULL) {
                throw std::runtime_error("Couldn't open file for writing");
                return;
            }


            fclose(f);


            /*m_altered = true;
            put_object_callback_data data;


            data.contentLength = 0;
            S3PutObjectHandler putObjectHandler =
                    {
                            responseHandler,
                            &putObjectDataCallback
                    };


            S3BucketContext bucketContext;
            bool error = false;
            PutObject context(data, error);


            bucketContext.accessKeyId       = m_access_key.c_str();
            bucketContext.secretAccessKey   = m_secret_key.c_str();
            bucketContext.authRegion        = nullptr;
            bucketContext.bucketName        = m_bucket_name.c_str();
            bucketContext.hostName          = m_host.c_str();
            bucketContext.protocol          = S3ProtocolHTTPS;
            bucketContext.uriStyle
             = S3UriStylePath;
            bucketContext.securityToken     = nullptr;


            S3_put_object(&bucketContext, m_uri.c_str(), 0, NULL, NULL, 0, &putObjectHandler, &context);
            fclose(data.infile);


            if(error) {
                LOGE << "Couldn't put empty file:" << m_uri;
            }*/
        }
        else
        {
            FILE *f = fopen(m_localfile.c_str(), "wb");
            if (f == NULL) {
                throw std::runtime_error("Couldn't open file for writing");
                return;
            }


            bool error = false;
            GetObject context(f, error);
            //LOGD << "Get object";
            S3_get_object(&bucketContext, m_uri.c_str(), NULL, 0, 0, NULL, 120000, &getObjectHandler, &context);
//            LOGD << "Get object end";
            if(error){
                fclose(f);
                remove(m_localfile.c_str());
                throw std::runtime_error("Couldn't download file");
                return;
            }


            fclose(f);
        }
    }
    else if (mode & io::ReadOnly)
    {
        if(!fileExists){
            throw std::runtime_error("Open readonly file but it doesn't exists:" + m_uri);
            return;
        }

        FILE *f = fopen(m_localfile.c_str(), "wb");
        if(!f){
            throw std::runtime_error("Couldn't open file for reading:" + m_uri);
            return;
        }


        bool error = false;


        GetObject context(f, error);
        S3BucketContext bucketContext;


        bucketContext.accessKeyId       = access_key.c_str();
        bucketContext.secretAccessKey   = secret_key.c_str();
        bucketContext.authRegion        = nullptr;
        bucketContext.bucketName        = bucket_name.c_str();
        bucketContext.hostName          = host.c_str();
        bucketContext.protocol          = S3ProtocolHTTPS;
        bucketContext.uriStyle          = S3UriStylePath;
        bucketContext.securityToken     = nullptr;


        S3_get_object(&bucketContext, m_uri.c_str(), NULL, 0, 0, NULL, 120000, &getObjectHandler, &context);

        if(error){
            fclose(f);
            remove(m_localfile.c_str());
            throw std::runtime_error("Couldn't download file");
            return;
        }
        fclose(f);
    }


     // calculate local file size
     if ((m_localsize = aux::getFileSize(m_localfile.c_str())) == -1){
         LOGE << "Couldn't calculate file size:" << m_localsize;
         return;//throw aux::InternalErrorException("local file calculate size failed");
     }


//     LOGD << "m_localsize:" << m_localsize;


}


S3IODevice::~S3IODevice()
{
   LOGD << "Close IODevice:" << m_uri << ", " << m_pos;


    flush();
    remove(m_localfile.c_str());
    //m_impl->Quit();
}


// Since there is no explicit 'close' function,
// synchronization attempt is made in this function and
// this function is called from destructor.
// That's why all possible errors are discarded.
void S3IODevice::flush()
{
    std::lock_guard<std::mutex> lock(m_mutex);


    if(m_altered) {
        put_object_callback_data data;


        struct stat statbuf;
        if (stat(m_localfile.c_str(), &statbuf) == -1) {
            LOGE << "Culdn't get file size:" << m_localfile;
            return;
        }


        int contentLength = statbuf.st_size;
        data.contentLength = contentLength;


        if (!(data.infile = fopen(m_localfile.c_str(), "r"))) {
            LOGE << "Couldn't open:" << m_localfile;
            return;
        }


        bool error = false;


        PutObject context(data);
        BaseContext base_context(error, &context);
        S3PutObjectHandler putObjectHandler =
                {
                        responseHandler,
                        &putObjectDataCallback
                };


        S3BucketContext bucketContext;


        bucketContext.accessKeyId       = m_access_key.c_str();
        bucketContext.secretAccessKey   = m_secret_key.c_str();
        bucketContext.authRegion        = nullptr;
        bucketContext.bucketName        = m_bucket_name.c_str();
        bucketContext.hostName          = m_host.c_str();
        bucketContext.protocol          = S3ProtocolHTTPS;
        bucketContext.uriStyle          = S3UriStylePath;
        bucketContext.securityToken     = nullptr;




        S3_put_object(&bucketContext, m_uri.c_str(), contentLength, NULL, NULL, 0, &putObjectHandler, &base_context);
        fclose(data.infile);
    }
}


uint32_t STORAGE_METHOD_CALL S3IODevice::write(
    const void     *src,
    const uint32_t  size,
    int            *ecode
)
{
        int res;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (ecode)
        *ecode = error::NoError;


    //LOGD << "********** Write:" << size << " to" << m_uri << ", m_pos:" << m_pos;


    if (!(m_mode & io::WriteOnly))
    {
        *ecode = error::WriteNotSupported;
        return 0;
    }


    FILE * f = fopen(m_localfile.c_str(), "r+b");
    if (f == NULL)
        goto bad_end;


    if (fseek(f, (int)m_pos, SEEK_SET) != 0)
        goto bad_end;


    res = fwrite(src, 1, size, f);
    m_pos += size;
    m_localsize += size;
    m_altered = true;
    fclose(f);
    //LOGD << "Write returns:" << res;
    return size;


bad_end:
    if (f != NULL)
        fclose(f);


    LOGE << "write:error";


    *ecode = error::UnknownError;
    return 0;
}


uint32_t STORAGE_METHOD_CALL S3IODevice::read(
    void*           dst,
    const uint32_t  size,
    int*            ecode
) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t readSize = 0;
    if (ecode)
        *ecode = error::NoError;


//    LOGD << "********* read:" << size << ", pos:" << m_pos << ", from:" << m_uri;


    if (!(m_mode & io::ReadOnly))
    {
        LOGE << "not support" << m_mode;
        *ecode = error::ReadNotSupported;
        return 0;
    }


    FILE * f = fopen(m_localfile.c_str(), "rb");
    if (f == NULL)
        goto bad_end;


    readSize = (uint32_t)(m_pos + size > m_localsize ? m_localsize - m_pos : size);


    if (fseek(f, (int)m_pos, SEEK_SET) != 0)
        goto bad_end;


    fread(dst, 1, readSize, f);
    m_pos += readSize;
    fclose(f);


//LOGD << "read return:" <<readSize;
    return readSize;


bad_end:
    if (f != NULL)
        fclose(f);
    *ecode = error::UnknownError;
    return 0;
}


int STORAGE_METHOD_CALL S3IODevice::seek(
    uint64_t    pos,
    int*        ecode
)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (ecode)
        *ecode = error::NoError;


  //  LOGD << "seek:" << pos;


    m_pos = pos;
    return 1;
}


int STORAGE_METHOD_CALL S3IODevice::getMode() const
{
    return m_mode;
}


uint32_t STORAGE_METHOD_CALL S3IODevice::size(int* ecode) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (ecode)
        *ecode = error::NoError;






    long long ret;
    if ((ret = aux::getFileSize(m_localfile.c_str())) == -1)
    {
        LOGE << "Couldn't get file size";
        if (ecode)
            *ecode = error::UnknownError;
        return 0;
    }
    //LOGD << "********** Get size:" << m_uri << ", " << ret;
    return static_cast<uint32_t>(ret);
}


void* S3IODevice::queryInterface(const nxpl::NX_GUID& interfaceID)
{
    if (std::memcmp(&interfaceID,
                    &IID_IODevice,
                    sizeof(nxpl::NX_GUID)) == 0)
    {
        addRef();
        return static_cast<nx_spl::IODevice*>(this);
    }
    else if (std::memcmp(&interfaceID,
                            &nxpl::IID_PluginInterface,
                            sizeof(nxpl::IID_PluginInterface)) == 0)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}


unsigned int S3IODevice::addRef()
{
    return p_addRef();
}


unsigned int S3IODevice::releaseRef()
{
    return p_releaseRef();
}


} // namespace nx_spl


extern "C"
{
#ifdef _WIN32
    __declspec(dllexport)
#endif
    nxpl::PluginInterface* createNXPluginInstance()
    {
        //static plog::RollingFileAppender<plog::TxtFormatter> fileAppender("/tmp/s3plugin.log", 100000, 3);
        //auto& logger = plog::init(plog::debug, &fileAppender);//.addAppender(&fileAppender);
        plog::init(plog::debug, "/var/log/nxs3plugin.log"); // Step2: initialize the logger.


        LOGD << "\n\n\n           ============== Start plugin";


        return new nx_spl::S3StorageFactory();
    }
}