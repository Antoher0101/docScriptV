#pragma once
#include <iostream>
#include "VK/src/api.h"
#include <fstream>
#include <thread>
#include <io.h>

#define VKS_LOGS
#define CONSOLE_LOG

#define VK_SIZE_OF_ONE_REQUEST 450 // This parameter is selected empirically.

#define VK_NORMAL 0x00
#define VK_ERROR 0x01
#define VK_EXTENDED 0x02
#define VK_WARNING 0x03
#define VK_SPECIAL 0x04

#define ull unsigned long long
namespace vkscript
{
	class VkScriptCore
	{
	private:
		VK::Client api;
		std::chrono::system_clock::time_point ztime;
		std::chrono::steady_clock::time_point last_request_time;
		static std::stringstream log;
		static std::string token_link;
		static std::vector<nlohmann::json> access_tokens;
		static std::string authorize_uri;
		std::string access_token;
		static std::string preselect_token;

		static std::string nullOauth(const std::string& link);
		static std::string setAccessToken(const std::string& link);
		
		static bool readTokenFile();
		static void saveToken(std::string name, std::string link);
		static void chooseToken();
		static void saveLog();
		bool checkAccess();

	protected:
		std::string user_name;
		nlohmann::ordered_json user_info;
		
		ull old_start;
		ull start_point;
		ull end_point;
		ull user_id;
		
		bool token_installed;
		bool is_user_found;

		virtual void addOtherUserInfo() = 0;
		virtual void readOtherUserInfo() = 0;
		virtual void setFilename() = 0;
		void readUserInfo();
		static void addLog(std::string str, int logtype = VK_NORMAL);
		nlohmann::json sendRequest(const std::string& method, const std::string& params = "");
		nlohmann::json sendRequest(const std::string& method, const VK::params_map& params);
		virtual std::string formatData(nlohmann::json& data, int mode = VK_NORMAL, const std::string& prefix = {}) = 0;
	public:
		VkScriptCore(const VkScriptCore&) = delete;
		VkScriptCore();
		virtual ~VkScriptCore();

		virtual bool start() = 0; // Main launch function
		virtual void stop() = 0;
		virtual nlohmann::json getResult()const = 0;
		virtual void saveResult() = 0;
		virtual std::string getFilename()const = 0;
		virtual void c_clear() = 0;
		static std::string addToken();
		
		int auth(const std::string& preselect = "");
		
		bool setUser(const std::string& id);

		ull getUserId() const;

		
		std::string getAccessToken() const;
		bool getTokenStatus();

		void clear();
	};
	// Utils
	std::string getDate();
	std::string getDate(ull unixtime);
	std::string cleanString(const std::string& str);
	std::string getElapsedTime(std::chrono::system_clock::time_point t0);
	double simple_jaccard(const std::string& first_str, const std::string& second_str);
}
