#pragma once

#include "../filesys/iblockdevice.hpp"
#include "../filesys/vfs/vfs.hpp"
#include "string.hpp"
#include <logging.hpp>

class ICharDevice;

#define MAX_BLOCK_DEVICE 16
#define MAX_CHAR_DEVICE 16

typedef U64 REQUEST;
typedef ANSI_STRING DEVICE_NAME;

typedef enum _DesiredAccess{
    ACC_READ = (1 << 1),
    ACC_WRITE = (1 << 2), 
} DesiredAccess;

typedef enum _ShareAccess{
    EXCLUSIVE = 0,
    FILE_SHARE_READ = (1 << 1)
} ShareAccess;

typedef enum _CreateDisposition{
    OPEN_EXISTING = 0,
    CREATE_NEW,
    TRUNCATE_EXISITING
} CreateDisposition;

namespace DeviceManager{
    extern IBlockDevice *g_BlockDevices[MAX_BLOCK_DEVICE];
    extern U32 g_BlockDeviceCount;

    extern ICharDevice *g_CharDevices[MAX_CHAR_DEVICE];
    extern U32 g_CharDeviceCount;

    BOOL RegisterBlockDevice(IBlockDevice *Device);

    BOOL UnregisterBlockDevice(IBlockDevice *Device);

    BOOL RegisterCharDevice(ICharDevice *Device);

    U32 GetBlockDeviceCount();
    IBlockDevice *GetBlockDevice(U32 Index);

    IBlockDevice* FindBlockDevice(const char* name);
    ICharDevice* FindCharDevice(const char* name);

    namespace StorageManager{
        VOID SyncAllStorageDevices();
    }
}

namespace DeviceManager{
    #define MAX_REQUEST_SLOT 32
    typedef VOID* (*HandleFunction)(POINTER Anything);
    struct _DispatchTable{
        REQUEST RequestID;
        HandleFunction Function;
    };
    enum ObjectType{
        OT_DIRECTORY,
        OT_CHAR,
        OT_BLOCK,
        OT_FILE,
        OT_DISPLAY,
        OT_NET,
        OT_DEVICE,
        OT_OTHER
    };
    struct DeviceObject{
        CHAR8 NameDevice[64];
        ObjectType Type;

        DeviceObject *Parent;
        DeviceObject *Child;
        DeviceObject *Sibling;

        Arch::Spinlock::Spinlock Lock;
        VOID *DriverInstance;
        _DispatchTable FunctionTableing[MAX_REQUEST_SLOT];
        U64 FunctionTableCount;
    };
    struct FileObject {
        DeviceObject* RelatedDevice; // Pointer ke Hardware Asli
        U32 GrantedAccess;           // User ini boleh ngapain aja?
        U64 CurrentOffset;           // Cursor posisi baca file (Seek)
        // ... pointer ke process owner ...
    };
    class ObjectManagerTree{
        private:
            DeviceObject *Root;

            DeviceObject *FindInDirectory(DeviceObject *Dir, CONSTANT CPANSI_STRING Name){
                if(!Dir || Dir->Type != OT_DIRECTORY) return nullptr;
                
                DeviceObject *Current = Dir->Child;
                while(Current){
                    if(String::Strcmp(Current->NameDevice, Name) == 0){
                        return Current;
                    }
                    Current = Current->Sibling;
                }
                return nullptr;
            }   

            DeviceObject *CreateDirectoryNode(DeviceObject *Parent, CPANSI_STRING Name){
                DeviceObject *NewDir = new DeviceObject();
                String::Memset(NewDir, 0, sizeof(DeviceObject));
                String::Strcpy(NewDir->NameDevice, Name);
                NewDir->Type = OT_DIRECTORY;
                NewDir->Parent = Parent;

                if(Parent->Child == nullptr){
                    Parent->Child = NewDir;
                } else {
                    NewDir->Sibling = Parent->Child;
                    Parent->Child = NewDir;
                }
                return NewDir;
            }

            BOOL IsOBJInitialized;
        public:
            ObjectManagerTree(){
                Root = new DeviceObject();
                String::Memset(Root, 0, sizeof(DeviceObject));
                String::Strcpy(Root->NameDevice, 0);
                Root->Type = OT_DIRECTORY;
            }

            DeviceObject *ParsePath(CPANSI_STRING FullPath, PANSI_STRING LeafNameOut){
                DeviceObject *CurrentDir = Root;

                ANSI_STRING Buffer[64];
                INTN PathIndex = 0;

                while(FullPath[PathIndex] != '\0'){
                    INTN i = 0;
                    while(FullPath[PathIndex] != '/' && FullPath[PathIndex] != '\0'){
                        Buffer[i++] = FullPath[PathIndex++];
                    }
                    Buffer[i] = 0;  

                    if(FullPath[PathIndex] == '/') PathIndex++;

                    if(FullPath[PathIndex] == '\0'){
                        String::Strcpy(LeafNameOut, Buffer);
                        return CurrentDir;
                    }

                    DeviceObject *NextDir = FindInDirectory(CurrentDir, Buffer);
                    if(!NextDir){
                        NextDir = CreateDirectoryNode(CurrentDir, Buffer);
                    }

                    CurrentDir = NextDir;
                }
                return nullptr;
            }

            ~ObjectManagerTree(){}
            BOOLFUNC FirstInitializeDevOBJManager(){
                IsOBJInitialized = TRUE;
                // Root selalu ada ("/")
                Root = new DeviceObject();
                String::Memset(Root, 0, sizeof(DeviceObject));
                String::Strcpy(Root->NameDevice, "/"); // Root name usually empty or slash
                Root->Type = OT_DIRECTORY;
                return TRUE;
            }

            BOOLFUNC CheckIfExist(CPANSI_STRING Path){
                FileObject* obj = OpenObject(Path, DesiredAccess::ACC_READ, ShareAccess::EXCLUSIVE, CreateDisposition::OPEN_EXISTING);
                return (obj != nullptr);
            }

            RHANDLE RegisterDevice(const char *Path, VOID *DriverPTR) {
                char DeviceName[64];
                
                // Cari folder parent ("dev/disk" dari "dev/disk/sda")
                DeviceObject* ParentDir = ParsePath(Path, DeviceName);
                if (!ParentDir) return nullptr; 

                // Cek duplikat di folder tersebut
                if (FindInDirectory(ParentDir, DeviceName)) return nullptr; 

                DeviceObject* NewDev = new DeviceObject();
                String::Memset(NewDev, 0, sizeof(DeviceObject));
                String::Strcpy(NewDev->NameDevice, DeviceName);
                NewDev->Type = OT_DEVICE;
                NewDev->DriverInstance = DriverPTR;
                NewDev->Parent = ParentDir;
                NewDev->FunctionTableCount = 0;

                // Insert ke List Sibling Parent
                NewDev->Sibling = ParentDir->Child;
                ParentDir->Child = NewDev;

                return NewDev;
            }

            REQUEST RegisterFunctionToTable(DeviceObject *Handle, REQUEST RequestID, HandleFunction FunctionHandle){
                if (!Handle) return 0;
                if(Handle->FunctionTableCount >= MAX_REQUEST_SLOT) return 0;

                Handle->FunctionTableing[Handle->FunctionTableCount].RequestID = RequestID;
                Handle->FunctionTableing[Handle->FunctionTableCount].Function = FunctionHandle;
                Handle->FunctionTableCount++;
                return RequestID;
            }

            FileObject *GiveInstance(CONSTANT ANSI_STRING *Path, FileObject **ObjOut){
                FileObject* obj = OpenObject(Path,
                     DesiredAccess::ACC_READ | DesiredAccess::ACC_WRITE,
                     ShareAccess::EXCLUSIVE, 
                     CreateDisposition::OPEN_EXISTING);
                if(ObjOut) *ObjOut = obj;
                return obj;
            }

            RHANDLE RequestStructOnDevice(DeviceObject* DevHandle, REQUEST ID, POINTER Struct){
                // 1. Validasi Handle
                if (!DevHandle) return nullptr;

                // 2. Cari Fungsi di Tabel Dispatch Driver
                // Kita harus Loop karena kamu pakai sistem Append (FunctionTableCount)
                for(U64 i = 0; i < DevHandle->FunctionTableCount; i++){
                    
                    // Cek apakah ID di slot ini cocok dengan yang diminta user?
                    if(DevHandle->FunctionTableing[i].RequestID == ID){
                        
                        // 3. Ambil Pointer Fungsinya
                        HandleFunction TargetFunc = DevHandle->FunctionTableing[i].Function;
                        
                        // 4. EKSEKUSI!
                        // Di sini magic-nya. Kita panggil fungsi driver, lempar struct-nya,
                        // dan tangkap nilai kembaliannya (RHANDLE/Memory Address).
                        if(TargetFunc) {
                            return TargetFunc(Struct); 
                        }
                    }
                }

                // Kalau loop selesai dan gak ketemu ID-nya
                return nullptr; 
            }

            FileObject* OpenObject(CPANSI_STRING Path, INTN Access, ShareAccess Share, CreateDisposition Disposition) {
                char TargetName[64];
                DeviceObject* DO = ParsePath(Path, TargetName);
                if(!DO) return nullptr;

                DeviceObject *TargetDevice = FindInDirectory(DO, TargetName);
                if(TargetDevice == nullptr){
                    // KASUS: Barangnya gada
                    // tapi gimana kalo disposition nya bilang
                    // mau bikin baru?
                    // 
                    // kita handle
                    if (Disposition == CREATE_NEW){
                        TargetDevice = new DeviceObject();
                        String::Strcpy(TargetDevice->NameDevice, TargetName);
                        TargetDevice->Parent = DO;
                        TargetDevice->Type = OT_DEVICE;
                        TargetDevice->Sibling = DO->Child;
                        DO->Child = TargetDevice;
                    } else {
                        return nullptr;
                    }
                }
                else {
                    if (Disposition == CREATE_NEW) {
                        // User minta CREATE_NEW (Wajib Baru), tapi barang udah ada.
                        // ERROR: File Exists!
                        return nullptr; 
                    }

                    if (Disposition == TRUNCATE_EXISITING) {
                        // User minta kosongin isi file lama.
                        // Call driver function buat set size jadi 0 here...
                    }
                }

                FileObject *HandleSession = new FileObject();
                HandleSession->RelatedDevice = TargetDevice;
                HandleSession->GrantedAccess = Access;
                return HandleSession;
            }

            DEVICEHANDLE IoControl(DeviceObject *DevHandle, REQUEST ID, POINTER Struct){
                return RequestStructOnDevice(DevHandle, ID, Struct);
            }

            // TODO THINGS TO ADD:
            // 1. HANDLE TABLE (ini penting banget)
            // 2. OpenObject sudah. tinggal bikin Write dan Read
            // 3. ?
    };
    extern ObjectManagerTree ObjectInstance;
}