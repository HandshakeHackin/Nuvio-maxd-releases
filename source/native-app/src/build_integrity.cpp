// SPDX-License-Identifier: GPL-3.0-or-later
#include "build_integrity.h"
#include "json.hpp"
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string_view>
#include <vector>
namespace build_integrity {
namespace {
using Json=nlohmann::json;
struct CloseFile {void operator()(FILE* file)const noexcept{if(file)std::fclose(file);}};
using File=std::unique_ptr<FILE,CloseFile>;
using Digest=std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)>;
using Key=std::unique_ptr<EVP_PKEY,decltype(&EVP_PKEY_free)>;
constexpr std::array<const char*,6> required={"CREDITS.txt","LICENSE","THIRD_PARTY.md","build-info.json","eboot.bin","sce_sys/param.json"};
struct Entry {std::string path;unsigned long long size;std::array<unsigned char,32> hash;};
bool hex(std::string_view value,unsigned char* out,std::size_t count) {
 if(value.size()!=count*2)return false;
 auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};
 for(std::size_t n=0;n<count;++n){int a=digit(value[2*n]),b=digit(value[2*n+1]);if(a<0||b<0)return false;out[n]=static_cast<unsigned char>(a*16+b);}return true;
}
bool relative_path(const std::string& path) {
 if(path.empty()||path.size()>240||path.front()=='/'||path.back()=='/')return false;
 for(unsigned char c:path)if(c<32||c>=127||c=='\\'||c==':')return false;
 std::size_t at=0;
 while(at<path.size()){auto end=path.find('/',at);auto part=path.substr(at,end==std::string::npos?end:end-at);if(part.empty()||part=="."||part=="..")return false;if(end==std::string::npos)break;at=end+1;}
 return true;
}
std::string text(const Json& j,const char* name){auto i=j.find(name);return i!=j.end()&&i->is_string()?i->get<std::string>():"";}
bool read_small(const std::string& path,std::size_t maximum,std::string& bytes) {
 File file(std::fopen(path.c_str(),"rb"));if(!file)return false;
 bytes.assign(maximum+1,'\0');auto count=std::fread(bytes.data(),1,bytes.size(),file.get());
 if(std::ferror(file.get())||count>maximum)return false;
 bytes.resize(count);return true;
}
}
Result verify(const std::string& root,const std::string& version,
              const std::array<unsigned char,32>& public_key,const std::atomic<bool>* cancel,unsigned budget_ms) noexcept {
 Result result;result.status=Status::unverified;
 auto fail=[&](Reason reason){result.reason=reason;return result;};
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(std::min(budget_ms,30000u));
 auto stopped=[&](){
  if(cancel&&cancel->load()){result.status=Status::cancelled;result.reason=Reason::none;return true;}
  if(std::chrono::steady_clock::now()>=deadline){result.reason=Reason::deadline;return true;}
  return false;
 };
 try {
  if(stopped())return result;
  std::string manifest,signature;
  if(!read_small(root+"/release-manifest.json",max_manifest,manifest))return fail(Reason::missing_manifest);
  if(!read_small(root+"/release-manifest.sig",64,signature)||signature.size()!=64)return fail(Reason::invalid_signature);
  Key key(EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519,nullptr,public_key.data(),public_key.size()),EVP_PKEY_free);
  Digest context(EVP_MD_CTX_new(),EVP_MD_CTX_free);
  if(!key||!context||EVP_DigestVerifyInit(context.get(),nullptr,nullptr,nullptr,key.get())!=1)return fail(Reason::crypto);
  if(EVP_DigestVerify(context.get(),reinterpret_cast<const unsigned char*>(signature.data()),signature.size(),reinterpret_cast<const unsigned char*>(manifest.data()),manifest.size())!=1)return fail(Reason::invalid_signature);
  if(EVP_MD_CTX_reset(context.get())!=1)return fail(Reason::crypto);
  if(stopped())return result;
  auto j=Json::parse(manifest,nullptr,false);
  std::array<unsigned char,20> commit{};
  if(!j.is_object()||j.size()!=6||!j.contains("format")||!j["format"].is_number_integer()||j["format"]!=1||
     text(j,"project")!=project||text(j,"titleId")!="PPSA99288"||text(j,"version")!=version||
     !hex(text(j,"sourceCommit"),commit.data(),commit.size())||!j.contains("files")||!j["files"].is_array()||
     j["files"].size()<required.size()||j["files"].size()>max_files)return fail(Reason::invalid_manifest);
  std::vector<Entry> entries;unsigned long long total=0;std::string previous;
  std::array<bool,required.size()> present{};
  for(const auto& row:j["files"]) {
   if(!row.is_object()||row.size()!=3||!row.contains("size")||!row["size"].is_number_unsigned())return fail(Reason::invalid_manifest);
   Entry entry{text(row,"path"),row["size"].get<unsigned long long>(),{}};
   if(!relative_path(entry.path)||entry.path<=previous||entry.path=="release-manifest.json"||entry.path=="release-manifest.sig"||
      entry.size>max_file||total>max_total-entry.size||!hex(text(row,"sha256"),entry.hash.data(),entry.hash.size()))return fail(Reason::invalid_manifest);
   for(std::size_t n=0;n<required.size();++n)if(entry.path==required[n]){if(!entry.size)return fail(Reason::invalid_manifest);present[n]=true;}
   total+=entry.size;previous=entry.path;entries.push_back(std::move(entry));
  }
  if(std::find(present.begin(),present.end(),false)!=present.end())return fail(Reason::invalid_manifest);
  std::vector<unsigned char> buffer(64*1024);
  for(const auto& entry:entries) {
   if(stopped())return result;
   File file(std::fopen((root+"/"+entry.path).c_str(),"rb"));if(!file)return fail(Reason::unreadable_file);
   if(EVP_DigestInit_ex(context.get(),EVP_sha256(),nullptr)!=1)return fail(Reason::crypto);
   unsigned long long bytes=0;
   while(true) {
    if(stopped())return result;
    auto request=static_cast<std::size_t>(std::min<unsigned long long>(buffer.size(),entry.size-bytes+1));
    auto count=std::fread(buffer.data(),1,request,file.get());
    if(std::ferror(file.get()))return fail(Reason::unreadable_file);
    if(count>entry.size-bytes)return fail(Reason::changed_file);
    if(!count)break;
    bytes+=count;if(EVP_DigestUpdate(context.get(),buffer.data(),count)!=1)return fail(Reason::crypto);
   }
   std::array<unsigned char,32> digest{};unsigned length=0;
   if(EVP_DigestFinal_ex(context.get(),digest.data(),&length)!=1||length!=digest.size())return fail(Reason::crypto);
   if(bytes!=entry.size||CRYPTO_memcmp(digest.data(),entry.hash.data(),digest.size())!=0)return fail(Reason::changed_file);
   ++result.files_checked;
  }
  if(stopped())return result;
  result.status=Status::verified;result.reason=Reason::none;result.credits_checked=true;return result;
 } catch(...) {return fail(Reason::invalid_manifest);}
}
}
