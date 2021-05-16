#pragma once
#include "VK/src/api.h"
#include <fstream>
#include <thread>

#define _LOGS
#define CONSOLE_LOG

#define TIME_RQ	334

#define _SIZE_OF_ONE_REQUEST 450 // This parameter is selected empirically.

#define VK_NORMAL 0x00
#define VK_ERROR 0x01
#define VK_EXTENDED 0x02
#define VK_WARNING 0x03
#define VK_SPECIAL 0x4

namespace vkscript
{
	class VkScriptCore
	{
		VK::Client api;

		std::string user_name;
		std::string access_token;
		std::string result;
		nlohmann::json backup;

		//First ID in the array
		unsigned long long current_id;
		//Last ID in the array
		unsigned long long last_checked_id;

		unsigned long long old_start;
		unsigned long long start_point;
		unsigned long long end_point;
		unsigned long long user_id;

		double percent;

		bool searching;
		bool token_installed;

		std::vector<std::vector<long long>> id_arrays;

		void saveInterruptedSearch(unsigned long long breakpoint);
		void startSearching();
		void genIds();
		std::string idToString(std::vector<long long>) const;

		static std::stringstream log;
		static std::string token_link;
		static bool reset_token;

		static void addLog(std::string str, int logtype = VK_NORMAL);
		static void saveLog();

		static std::string setAccessToken(const std::string& link);
		static bool checkTokenFile();
		static void saveToken(std::string link);

		static std::string formatData(nlohmann::json& data, int mode = VK_NORMAL);
	public:
		VkScriptCore(const VkScriptCore&) = delete;
		VkScriptCore();
		~VkScriptCore();

		void start(); // Main launch function
		void stop();

		void reauth();
		void auth();

		void saveResult();
		void loadBackup(const char* path);

		void setStartPoint(long long n);
		void setEndPoint(long long n);
		bool setUserId(const std::string& id);

		nlohmann::json getResult()const;
		double getPercent()const;
		unsigned long long getUserId() const;
		bool getSearchStatus() const;
		std::string getProgramTime() const;
		std::string getAccessToken() const;

		bool getTokenStatus();
	};
	// Utils
	std::string getDate();
	std::string getDate(unsigned long long unixtime);	
}