#include "VkScriptCore.h"

std::string vkscript::VkScriptCore::token_link;
std::stringstream vkscript::VkScriptCore::log;
bool vkscript::VkScriptCore::reset_token;

vkscript::VkScriptCore::VkScriptCore() : api("5.21")
{
	addLog("VkScriptCore created.");
	searching = false;
}

void vkscript::VkScriptCore::auth()
{
	if (api.oauth(setAccessToken)) {
		access_token = api.access_token();
		token_installed = true;
	}
	else
	{
		token_installed = false;
		addLog("Failed to set token", VK_ERROR);
	}
}

void vkscript::VkScriptCore::reauth()
{
	reset_token = true;
	auth();

	reset_token = false;
}

void vkscript::VkScriptCore::start()
{
	genIds();
	startSearching();
}

void vkscript::VkScriptCore::stop()
{
	searching = false;
	saveInterruptedSearch(current_id);
}

void vkscript::VkScriptCore::startSearching()
{
	addLog("Start searching...");
	
	percent = 0;
	searching = true;
	const double end = end_point;
	double interval;
	old_start == 0 ? interval = start_point - end_point : interval = old_start - end_point;
	while (searching)
	{
		for (auto a : id_arrays)
		{
			last_checked_id = a.back();
			current_id = a.front();
			
			std::string params = idToString(a);

			auto clk_st = std::chrono::high_resolution_clock::now();
			VK::json data = api.call("docs.getById", params);

			auto clk_end = std::chrono::high_resolution_clock::now();

			auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(clk_end - clk_st).count();
			if (elapsed_time < TIME_RQ)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(TIME_RQ - elapsed_time));
			}
			
			double progress = 100 - round(((last_checked_id - end) / interval) * 100.0);

			data["interval"] =  std::to_string(current_id) + "-" + std::to_string(last_checked_id);

			if (data["error"].empty()) {
				if (!data["response"].empty()) {
					result += formatData(data, VK_EXTENDED);
					addLog("Something was found.", VK_SPECIAL);
				}
			}
			else
			{
				std::string error = formatData(data, VK_ERROR);
				addLog(error, VK_ERROR);
				
				saveInterruptedSearch(current_id);
				break;
			}
			if (progress == 100) progress = 99;
			if (percent < progress)
				std::cout << progress << "%" << std::endl;
			last_checked_id != end_point ? percent = progress : percent = 100;  // Rounding correction
		}
		searching = false;
	}
	addLog("Search completed.");
	searching = false;
}

bool vkscript::VkScriptCore::checkTokenFile()
{
	if (reset_token) return false;
	std::string temp;
	std::ifstream read_token("token", std::ios_base::app);
	if (read_token.is_open()) {
		read_token >> temp;
		read_token.close();
	}
	if (!temp.empty()) {
		token_link = temp;
		addLog("Token found in file.");
		return true; // token already installed
	}
	return false;
}
// Callback function for install a token
std::string vkscript::VkScriptCore::setAccessToken(const std::string& link)
{
	std::string res;
	if (!checkTokenFile()) {
		std::string com = "start \"\" ";
		com += "\"" + link + "\"";
		system(com.c_str());
		// Link sends to the get the token page
		std::cout << link << std::endl;
		std::cin >> res;
		saveToken(res);
	}
	else {
		res = token_link;
	}
	return res;
}

void vkscript::VkScriptCore::saveToken(std::string link)
{
	std::ofstream write_token("token");
	if (write_token.is_open())
	{
		write_token << link;
		write_token.close();

		addLog("Token saved.");
	}
}

std::string vkscript::VkScriptCore::formatData(nlohmann::json& data, int mode)
{
	std::string fdata;
	if (mode == VK_NORMAL)
	{
		nlohmann::basic_json<> inf = data["response"][0];
		fdata += "{\nTitle: " + inf["title"].get<std::string>() + "\n"
			+ "Date: " + getDate(inf["date"].get<unsigned long long>()) + "\n"
			+ "URL: " + inf["url"].get<std::string>() + "\n"
			+ "Interval: " + data["interval"].get<std::string>() + "\n"
			+ "}\n";
	}
	else if (mode == VK_ERROR)
	{
		nlohmann::basic_json<> err = data["error"];
		fdata += "(" + data["interval"].get<std::string>() + ") "
			+ "Error code: "
			+ err["error_code"].dump() + ". " // Warning! Is not a string
			+ err["error_msg"].get<std::string>();
	}
	else if (mode == VK_EXTENDED)
	{
		nlohmann::basic_json<> inf = data["response"][0];
		fdata +="{\nTitle: " + inf["title"].get<std::string>() + "\n"
			+ "Owner ID: " + inf["owner_id"].dump() + "\n"
			+ "ID: " + inf["id"].dump() + "\n"
			+ "Date: " + getDate(inf["date"].get<unsigned long long>()) + "\n"
			+ "Type: " + inf["ext"].get<std::string>() + "\n"
			+ "Size: " + inf["size"].dump() + "byte" + "\n"
			+ "URL: " + inf["url"].get<std::string>() + "\n"
			+ "Interval: " + data["interval"].get<std::string>() + "\n"
			+ "}\n";
	}
	return fdata;
}

std::string vkscript::VkScriptCore::idToString(std::vector<long long> a) const
{
	std::string ids = "docs=";
	const std::string ui= std::to_string(user_id);
	for (auto id : a)
	{
		ids += ui + "_" + std::to_string(id)+ ",";
	}
	ids.pop_back();
	return ids;
}

void vkscript::VkScriptCore::genIds()
{
	addLog("Creating an array of search IDs...");
	if (start_point >= end_point)
	{
		const unsigned long long search_interval = start_point - end_point;
		const int size_of_one_request = _SIZE_OF_ONE_REQUEST;

		auto current_id = start_point;
		unsigned int i = static_cast<unsigned int>(ceil(static_cast<double>(search_interval) / static_cast<double>(size_of_one_request)));
		for (; i > 0; i--)
		{
			std::vector<long long> id_set;
			for (int j = size_of_one_request; j > 0; j--, current_id--)
			{
				if (current_id >= end_point)
					id_set.emplace_back(current_id);
			}
			id_arrays.push_back(id_set);
		}
		addLog("Array creation completed successfully.");
	}
	else {
		addLog("Wrong search interval.", VK_ERROR);
		exit(0);
	}

}

void vkscript::VkScriptCore::saveInterruptedSearch(unsigned long long breakpoint)
{
	nlohmann::json bkp{};
	old_start != 0 ? bkp["start_point"] = old_start : bkp["start_point"] = start_point;
	bkp["end_point"] = end_point;
	bkp["user_id"] = user_id;
	bkp["breakpoint"] = breakpoint;
	
	std::string filename = "backups//" + std::to_string(user_id) + ".json";
	
	std::ofstream outdata(filename, std::ios::trunc);
	if (outdata.is_open())
	{
		outdata << bkp;
		outdata.close();
		addLog("Search recovery data has been saved in json file.");
	}
	else
	{
		addLog("Search recovery data is not saved.", VK_WARNING);
	}
}

void vkscript::VkScriptCore::saveResult()
{
	std::string filename = "results//" + std::to_string(user_id) + ".txt";
	
	std::ofstream outdata(filename, std::ios::app);
	if (outdata.is_open())
	{
		outdata << result;
		outdata.close();
		addLog({ "Result has been saved in " + filename });
	}
}

void vkscript::VkScriptCore::loadBackup(const char* path)
{
	std::stringstream save_data;
	std::ifstream bkp(path, std::ios_base::in);
	if (bkp.is_open())
	{
		save_data << bkp.rdbuf();
		bkp.close();
	}
	nlohmann::json j = nlohmann::json::parse(save_data);
	old_start = j["start_point"].get<unsigned long long>();
	end_point = j["end_point"].get<unsigned long long>();
	user_id = j["user_id"].get<unsigned long long>();
	start_point = j["breakpoint"].get<unsigned long long>();
	addLog("Backup has been loaded.");
}

void vkscript::VkScriptCore::setStartPoint(long long n)
{
	start_point = n;
	addLog({ "Start point " + std::to_string(n) + " is set." });
}

void vkscript::VkScriptCore::setEndPoint(long long n)
{
	end_point = n;
	addLog({ "End point " + std::to_string(n) + " is set." });
}

bool vkscript::VkScriptCore::setUserId(const std::string &id) 
{
	if (id.find_first_not_of("0123456789") == std::string::npos) // Input consists only of numbers
	{
		user_id = stoi(id);
		addLog("User ID has been installed.");
		return true;
	}
	if (id.find_first_of('/') != std::string::npos)
	{
		std::string c_id = id;
		if (c_id[c_id.size() - 1] == '/') c_id.pop_back();
		auto sl = c_id.find_last_of('/') + 1;
		c_id.erase(c_id.begin(), c_id.begin() + sl);

		VK::params_map p = { {"q", c_id},
			{"count","1"},
		};

		VK::json data = api.call("users.search", p);
		try
		{
			user_id = data["response"]["items"][0]["id"].get<unsigned long long>();
			// User [First Last] : id1234567890 has been found.
			user_name = data["response"]["items"][0]["first_name"].get<std::string>() + " " + data["response"]["items"][0]["last_name"].get<std::string>();
			addLog({"User ["
				+ user_name
				+ "] : id" + std::to_string(data["response"]["items"][0]["id"].get<unsigned long long>())
				+ " has been found." });
			return true;
		}
		catch (...)
		{
			user_id = NULL;
			
			addLog("User not found", VK_WARNING);
			return false;
		}
	}
	else
	{
		VK::params_map p = { {"q", id},
			   {"count","1"},
		};
		VK::json data = api.call("users.search", p);
		try
		{
			user_id = data["response"]["items"][0]["id"].get<unsigned long long>();
			// User [First Last] : id1234567890 has been found.
			user_name = data["response"]["items"][0]["first_name"].get<std::string>() + " " + data["response"]["items"][0]["last_name"].get<std::string>();
			addLog({ "User ["
				+ user_name
				+ "] : id" + std::to_string(data["response"]["items"][0]["id"].get<unsigned long long>())
				+ " has been found." });
			return true;
		}
		catch (...)
		{
			addLog("User not found", VK_WARNING);
			return false;
		}
	}
}

nlohmann::json vkscript::VkScriptCore::getResult() const
{
	return result;
}

double vkscript::VkScriptCore::getPercent() const
{
	return percent;
}

unsigned long long vkscript::VkScriptCore::getUserId() const
{
	return user_id;
}

bool vkscript::VkScriptCore::getSearchStatus() const
{
	return searching;
}

bool vkscript::VkScriptCore::getTokenStatus()
{
	if (checkTokenFile())
	{
		token_installed = true;
	}
	else token_installed = false;
	return token_installed;
}

std::string vkscript::VkScriptCore::getProgramTime() const
{
	clock_t t = clock();
	return std::to_string(t / CLOCKS_PER_SEC);
}

std::string vkscript::VkScriptCore::getAccessToken() const
{
	return access_token;
}

void vkscript::VkScriptCore::saveLog()
{
	std::string filename = "logs//" + getDate();
	std::for_each(filename.begin(), filename.end(), [](char& l) {if (l == '.' || l == ':' || l == ' ')
	{
		l = '_';
	}});
	filename += ".log";

	std::ofstream outdata(filename);
	if (outdata.is_open())
	{
		outdata << log.rdbuf();
		outdata.close();
	}
}

void vkscript::VkScriptCore::addLog(std::string str, int logtype)
{
	std::string lgt;
	if (logtype == VK_NORMAL) lgt = "[INFO] ";
	if (logtype == VK_ERROR) lgt = "[ERROR] ";
	if (logtype == VK_WARNING) lgt = "[WARNING] ";
	if (logtype == VK_SPECIAL) lgt = "[FOUND] ";
	
	log << "[" << getDate() << "] " << lgt << str << std::endl;
#ifdef CONSOLE_LOG
	if (logtype == VK_NORMAL) std::cout  << str << std::endl;
	if (logtype == VK_ERROR) std::cout << "\x1b[31m" << str << "\x1b[0m" << std::endl;
	if (logtype == VK_WARNING) std::cout << "\x1b[33m" << str << "\x1b[0m" << std::endl;
	if (logtype == VK_SPECIAL) std::cout << "\x1b[35m" << str << "\x1b[0m" << std::endl;
#endif
}

vkscript::VkScriptCore::~VkScriptCore()
{
	addLog({"VkScriptCore finished in " + getProgramTime() + " seconds."});
#ifdef _LOGS
	saveLog();
#endif
}

std::string vkscript::getDate()
{
	char date[80];
	time_t seconds = time(NULL);
	struct tm timeinfo;
	localtime_s(&timeinfo, &seconds);
	const char* format = "%d.%m.%y %H:%M:%S";
	strftime(date, 80, format, &timeinfo);
	return date;
}
std::string vkscript::getDate(unsigned long long unixtime)
{
	char date[80];
	time_t seconds(unixtime);
	struct tm timeinfo;
	localtime_s(&timeinfo, &seconds);
	const char* format = "%d %b %Y %H:%M:%S";
	strftime(date, 80, format, &timeinfo);
	return  date;
}