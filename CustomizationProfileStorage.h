#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <string>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <cstdint>
#pragma comment(lib, "bcrypt.lib")

// Shared source for independent extensions. No owned-car tables, editor ABI,
// MinHook installation, or Unlimiter runtime is required to use this layer.
namespace CustomizationProfileStorage {
static uintptr_t GlobalAddress(uintptr_t address) {
#ifdef MWCL_SAVE_TEST_ADDRESS
    return MWCL_SAVE_TEST_ADDRESS(address);
#else
    return address;
#endif
}
static void Log(const char* message) noexcept {
    OutputDebugStringA("[MWCL Save] ");OutputDebugStringA(message);OutputDebugStringA("\n");
#ifndef MWCL_SAVE_TEST_ADDRESS
    // Isolated fixtures must not append simulated errors/events to the live
    // game's diagnostics; that obscures real ownership and save-flow evidence.
    // Diagnostics must never interrupt a native save or leak a handle on allocation failure.
    try {
        wchar_t local[MAX_PATH]{};
        const DWORD length=GetEnvironmentVariableW(L"LOCALAPPDATA",local,MAX_PATH);
        if(!length||length>=MAX_PATH)return;
        auto directory=std::filesystem::path(local)/L"NFSMWCustomizationLab";
        const auto target=directory/L"save.log";
        const std::string line=std::string(message)+"\r\n";
        CreateDirectoryW(directory.c_str(),nullptr);
        HANDLE file=CreateFileW(target.c_str(),FILE_APPEND_DATA,
            FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE)return;
        DWORD written=0;
        WriteFile(file,line.data(),DWORD(line.size()),&written,nullptr);CloseHandle(file);
    } catch(...) {}
#endif
}
static bool ValidName(const std::string& name) {
    return !name.empty() && name.size()<32 && name!="." && name!=".." &&
        name.find_first_of("\\/:*?\"<>|")==std::string::npos &&
        std::all_of(name.begin(),name.end(),[](unsigned char c){return c>=32;});
}
static std::string Name(const char* text) {
    if(!text) return {};
    size_t count=strnlen_s(text,32);
    return count<32 ? std::string(text,count) : std::string();
}
static std::string CompletedProfileName(const char* filename) {
    if(!filename)return {};
    const size_t size=strnlen_s(filename,1024);
    if(size==1024)return {};
    // PC callbacks may identify the completed file by a full path.
    std::string name(filename,size);
    const auto slash=name.find_last_of("\\/");
    if(slash!=std::string::npos)name.erase(0,slash+1);
    return ValidName(name)?name:std::string();
}
// Resolve the two observed PC call forms without bypassing call-site redirects.
static uintptr_t FolderCallTarget(const BYTE* call,uintptr_t imported) {
    uint32_t operand=0;std::memcpy(&operand,call+2,sizeof(operand));
    if(call[0]==0xFF&&call[1]==0x15&&operand==0x8902EC)return imported;
    if(call[0]==0xE8&&call[5]==0x90) {
        int32_t offset=0;std::memcpy(&offset,call+1,sizeof(offset));
        return reinterpret_cast<uintptr_t>(call)+5+offset;
    }
    return 0;
}
static std::filesystem::path ProfileDirectory(const std::string& name) {
    if(!ValidName(name)) return {};
    // Follow the call used by the game, including call-site save redirection.
    using FolderFn=HRESULT(WINAPI*)(HWND,int,HANDLE,DWORD,LPSTR);
#ifdef MWCL_SAVE_TEST_ADDRESS
    auto folder=*reinterpret_cast<FolderFn*>(GlobalAddress(0x8902EC));
#else
    auto folder=reinterpret_cast<FolderFn>(FolderCallTarget(
        reinterpret_cast<const BYTE*>(0x6CBF17),*reinterpret_cast<uintptr_t*>(0x8902EC)));
    if(!folder){Log("Unknown native save-directory call; sidecar I/O disabled.");return {};}
#endif
    char documents[MAX_PATH]{};
    if(!folder || FAILED(folder(nullptr,0x8005,nullptr,0,documents))) return {};
    const char* title=*reinterpret_cast<const char**>(GlobalAddress(0x901748));
    std::string directory=Name(title);
    if(!ValidName(directory)) return {};
    return std::filesystem::path(documents)/directory/name;
}
static uint32_t Crc(const void* data,size_t size) {
    uint32_t crc=0xFFFFFFFFu;
    auto bytes=static_cast<const unsigned char*>(data);
    for(size_t n=0;n<size;++n) {
        crc^=bytes[n];
        for(int bit=0;bit<8;++bit) crc=(crc>>1)^((0u-(crc&1))&0xEDB88320u);
    }
    return ~crc;
}
static bool Digest(const std::filesystem::path& file,std::array<uint8_t,32>& result) {
    std::ifstream stream(file,std::ios::binary);
    if(!stream) return false;
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    BCRYPT_HASH_HANDLE hash=nullptr;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0) return false;
    bool ok=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0;
    unsigned char buffer[4096]; size_t total=0;
    while(ok && stream) {
        stream.read(reinterpret_cast<char*>(buffer),sizeof(buffer));
        auto bytes=stream.gcount(); total+=size_t(bytes);
        if(total>16*1024*1024) {ok=false;break;}
        if(bytes && BCryptHashData(hash,buffer,ULONG(bytes),0)<0) ok=false;
    }
    ok=ok && stream.eof() && total>0 && BCryptFinishHash(hash,result.data(),ULONG(result.size()),0)>=0;
    if(hash) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm,0);
    return ok;
}
static DWORD* CurrentProfile() {
    auto database=*reinterpret_cast<DWORD**>(GlobalAddress(0x91CF90));
    return database ? reinterpret_cast<DWORD*>(database[4]) : nullptr;
}
}
