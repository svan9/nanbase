#ifndef NANVM_ISOLATE_HPP
#define NANVM_ISOLATE_HPP

#include "mewlib.h"
#include "mewtypes.h"
#include "mewmap"
#include "mewstack"
#include <stdio.h>
#include <stdbool.h>

namespace Virtual {
	constexpr const u64 stack_word = sizeof(uint);

	struct IsolateFile {
		mew::stack<u8> data;
	};
	
	class Isolate {
		typedef mew::map<const char*, IsolateFile>  isolate_disk_t;
	private:
		bool m_is_isolate = false;
		isolate_disk_t m_space;
		mew::stack<const char*> m_opened_files;
	public:
		Isolate() {}
		Isolate(bool is_isolate): m_is_isolate(is_isolate) {}

		bool IsExist(const char* path) {
			if (m_is_isolate) {
				return m_space.contains(path);
			} else {
				FILE *fp = fopen(path, "r");
				bool is_exist = false;
				if (fp != NULL) {
					is_exist = true;
					fclose(fp);
				}
				return is_exist;
			}
		}

		u32 Open(const char* path) {
			MewUserAssert(IsFile(path), "is not a path");
			if (m_is_isolate) {
				m_opened_files.push(path);
				return (u32)(m_opened_files.count() - 1);
			} 
			FILE* fp = fopen(path, "rb+");
			MewUserAssert(fp != nullptr, "failed to open file");
			return (u32)_fileno(fp);
		}

		bool Close(u32 descriptor) {
			if (m_is_isolate) {
				MewUserAssert(descriptor < m_opened_files.count(), "invalid descriptor");
				m_opened_files.erase((u64)descriptor);
				return true;
			} 
			FILE* fp = _fdopen(descriptor, "rb+");
			if (fp == nullptr) {
				return false;
			}
			int result = fclose(fp);
			return result == 0;
		}

		bool IsFile(const char* path) {
			if (m_is_isolate) {
				return m_space.contains(path);
			} else {
				FILE* fp = fopen(path, "rb");
				if (fp == nullptr) return false;
				bool is_file = (fseek(fp, 0, SEEK_SET) == 0);
				fclose(fp);
				return is_file;
			}
		}
		
		void WriteToFile(u64 descriptor, byte* data, u64 size) {
			if (m_is_isolate) {
				MewUserAssert(descriptor < m_opened_files.count(), "invalid descriptor");
				auto& file = m_space.at(m_opened_files[descriptor]);
				file.data.resize(size);
				memcpy(file.data.begin(), data, size);
				return;
			}
			FILE* fp = _fdopen((int)descriptor, "rb+");
			MewUserAssert(fp != nullptr, "failed to get file from descriptor");
			fwrite(data, 1, size, fp);
		}

		void WriteToFile(const char* path, const char* content) {
			MewUserAssert(IsFile(path), "is not a path");
			if (m_is_isolate) {
				auto& file = m_space.at(path);
				file.data.resize(strlen(content));
				memcpy(file.data.begin(), content, strlen(content));
				return;
			}
			FILE* fp = fopen(path, "wb");
			if (fp == nullptr) {
				return;
			}
			fwrite(content, 1, strlen(content), fp);
			fclose(fp);
		}

		void ReadFromFile(u64 descriptor, byte* dest, u64 size) {
			if (m_is_isolate) {
				MewUserAssert(descriptor < m_opened_files.count(), "invalid descriptor");
				auto& file = m_space.at(m_opened_files[descriptor]);
				MewUserAssert(file.data.count() >= size, "read size exceeds file size");
				memcpy(dest, file.data.begin(), size);
				return;
			}
			FILE* fp = _fdopen((int)descriptor, "rb");
			MewUserAssert(fp != nullptr, "failed to get file from descriptor");
			fread(dest, 1, size, fp);
		}


		const char* ReadFromFile(const char* path, size_t* fsize = nullptr) {
			MewUserAssert(IsFile(path), "is not a path");
			if (m_is_isolate) {
				auto& file = m_space.at(path);
				if (fsize) { *fsize = file.data.size();}
				return (const char*)file.data.begin();
			}
			FILE* fp = fopen(path, "rb");
			if (fp == nullptr) {
				return nullptr;
			}
			fseek(fp, 0, SEEK_END);
			long size = ftell(fp);
			if (fsize) { *fsize = size; }
			rewind(fp);
			char* buffer = new char[size + 1];
			size_t read_size = fread(buffer, 1, size, fp);
			buffer[read_size] = '\0';
			fclose(fp);
			return buffer;
		}
		
		void CreateFileIfNotExist(const char* path) {
			if (IsExist(path)) return;
			if (m_is_isolate) {
				IsolateFile file;
				m_space.insert(path, file);
			} else {
				FILE* fp = fopen(path, "wb");
				if (fp != nullptr) {
					fclose(fp);
				}
			}
		}

		static void CreateFileIfNotExists(const char* path) {
			if (mew::is_exists(path)) return;
			FILE* fp = fopen(path, "wb");
			if (fp != nullptr) {
				fclose(fp);
			}
		}
	};
};

#endif