#include "VkScriptCore.h"

std::vector<nlohmann::json> vkscript::VkScriptCore::access_tokens;
std::string vkscript::VkScriptCore::authorize_uri;
std::string vkscript::VkScriptCore::token_link;
std::stringstream vkscript::VkScriptCore::log;
std::string vkscript::VkScriptCore::preselect_token;

vkscript::VkScriptCore::VkScriptCore() : api("5.21")
{
	ztime = std::chrono::system_clock::now();
	addLog("VkScriptCore created.");
	api.oauth(nullOauth);
}

std::string vkscript::VkScriptCore::nullOauth(const std::string& link)
{
	authorize_uri = link;
	return {};
}

int vkscript::VkScriptCore::auth(const std::string& preselect)
{
	preselect_token = preselect;
	if (api.oauth(setAccessToken)) {
		access_token = api.access_token();
		token_installed = checkAccess();
		return 0;
	}
	token_installed = false;
	addLog("Failed to set token", VK_ERROR);
	return 1;
}

void vkscript::VkScriptCore::readUserInfo()
{
	// Mandatory user information
	user_info["User info"]["Username"] = user_name;
	user_info["User info"]["User ID"] = user_id;
	
	const std::string filename = "results/" + getFilename() + ".txt";
	std::ifstream read_inf(filename, std::ios::in);
	if (read_inf.is_open())
	{
		std::string inf;
		for (std::string str; std::getline(read_inf, str);)
		{
			inf += str;
			if (std::count(inf.begin(), inf.end(), '}') >= 2) { while (inf[inf.size() - 1] != '}') { inf.pop_back(); } break; }
		}
		if (inf.empty() || inf.find("User info") == std::string::npos)
		{
			
			addLog("User info not found in a file. It will be created after the operation is complete.", VK_WARNING);
		}
		else {
			// Write the read information from the file
			try { user_info = nlohmann::ordered_json::parse(inf); }
			catch(...) {addLog("Reading user information from a file caused an error.", VK_WARNING); }
			try { readOtherUserInfo(); }
			catch (...) {
				addLog("Additional user information not found in a file. It will be created after the operation is complete.", VK_WARNING);
			}
		}
		read_inf.close();
	}
}
bool vkscript::VkScriptCore::readTokenFile()
{
	std::ifstream read_token("tokens", std::ios::in);
	if (read_token.is_open())
	{
		for (std::string line; std::getline(read_token, line); )
		{
			nlohmann::json jt = nlohmann::json::parse(line);
			access_tokens.emplace_back(jt);
		}
		read_token.close();
		addLog("File contains " + std::to_string(access_tokens.size()) + " access tokens.");
	}
	if (access_tokens.size()>1)
	{
		chooseToken();
	}
	if (!access_tokens.empty()) {
		token_link = access_tokens[0]["token"].get<std::string>();
		return true; // token already installed
	}
	return false;
}

// Callback function for install a token
std::string vkscript::VkScriptCore::setAccessToken(const std::string& link)
{
	authorize_uri = link;
	std::string res;
	if (!readTokenFile()) {
		std::string com = "start \"\" ";
		com += "\"" + link + "\"";
		system(com.c_str());
		// Link sends to the get the token page
		std::cout << link << std::endl;
		res = addToken();
	}
	else {
		res = token_link;
	}
	return res;
}

std::string vkscript::VkScriptCore::addToken()
{
	std::string n;
	std::string t;
	std::cout << "Enter a token name: ";
	getline(std::cin, n);
	
	std::cout << authorize_uri << std::endl;
	
	std::cout << "\nEnter a token link: ";
	getline(std::cin, t);

	saveToken(n, t);
	return t;
}

void vkscript::VkScriptCore::saveToken(std::string name, std::string link)
{
	nlohmann::json token{};
	token["name"] = name;
	token["token"] = link;

	std::ofstream write_token("tokens", std::ios::app);
	if (write_token.is_open())
	{
		write_token << token << "\n";
		write_token.close();
		addLog({ "Token \"" + name + "\"has been saved." });
	}
}

void vkscript::VkScriptCore::chooseToken()
{
	// Jacquard coefficient just for fun
	if (!preselect_token.empty())
	{
		double k = 0.0;
		size_t choice = 0;
		for (size_t i = 0; i < access_tokens.size(); i++)
		{
			double tk = simple_jaccard(preselect_token, access_tokens[i]["name"].get<std::string>());
			if (k < tk) { k = tk; choice = i; }
		}
		std::swap(access_tokens[choice], access_tokens.front());
		addLog({ "Selected token - " + access_tokens[0]["name"].get<std::string>() });
		return;
	}
	
	size_t choice = 1;
	for (size_t i = 0; i < access_tokens.size(); i++)
	{ std::cout << "[" << i + 1 << "] " << access_tokens[i]["name"].get<std::string>() << std::endl; }
	// Checking the correctness of the input
	bool icorrect = false;
	while (!icorrect) {
		std::cout << "Select the number of the required token: ";
		if (!(std::cin >> choice))
		{
			std::cin.clear();
			std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
		}
		else --choice <= access_tokens.size() ? icorrect = true : icorrect = false;
	}
	std::swap(access_tokens[choice], access_tokens.front());
	addLog({ "Selected token - " + access_tokens[0]["name"].get<std::string>() });
}

bool vkscript::VkScriptCore::setUser(const std::string &id) 
{
	std::string strid = id;
	if (strid.find_first_not_of("0123456789") == std::string::npos) // Input consists only of numbers
	{
		strid.insert(0,"id");
	}
	if (strid.find_first_of('/') != std::string::npos)
	{
		if (strid[strid.size() - 1] == '/') strid.pop_back();
		auto sl = strid.find_last_of('/') + 1;
		strid.erase(strid.begin(), strid.begin() + sl);
	}
	VK::params_map p = { {"q", strid},
			{"count","1"},
		};
	VK::json data = sendRequest("users.search", p);
	if (!data["error"].empty())
	{
		addLog(formatData(data, VK_ERROR),VK_ERROR);
	}
	try
	{
		user_id = data["response"]["items"][0]["id"].get<unsigned long long>();
		// User [First Last] : id1234567890 has been found.
		user_name = data["response"]["items"][0]["first_name"].get<std::string>() + " " + data["response"]["items"][0]["last_name"].get<std::string>();
		setFilename();
		addLog({ "User ["
				+ user_name
				+ "] : id" + std::to_string(data["response"]["items"][0]["id"].get<unsigned long long>())
				+ " has been found." });
		readUserInfo();
		
		is_user_found = true;
		return true;
	}
	catch (...) {}
	user_id = NULL;
	addLog("User not found", VK_WARNING);
	is_user_found = false;
	return false;
}

nlohmann::json vkscript::VkScriptCore::sendRequest(const std::string& method, const std::string& params)
{
	last_request_time = std::chrono::high_resolution_clock::now();
	nlohmann::json data = api.call(method, params);
	std::chrono::steady_clock::time_point waitResponse = std::chrono::high_resolution_clock::now();
	// VK can only respond to 3 requests per second
	const long long elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(waitResponse - last_request_time).count();
	if (elapsed_time < 1000 / 3)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1000 / 3 - elapsed_time));
	}
	return data;
}
nlohmann::json vkscript::VkScriptCore::sendRequest(const std::string& method, const VK::params_map& params)
{
	last_request_time = std::chrono::high_resolution_clock::now();
	nlohmann::json data = api.call(method, params);
	std::chrono::steady_clock::time_point waitResponse = std::chrono::high_resolution_clock::now();
	// VK can only respond to 3 requests per second
	const long long elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(waitResponse - last_request_time).count();
	if (elapsed_time < 1000 / 3)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1000 / 3 - elapsed_time));
	}
	return data;
}

ull vkscript::VkScriptCore::getUserId() const
{
	return user_id;
}

bool vkscript::VkScriptCore::getTokenStatus()
{
	if (readTokenFile())
	{
		token_installed = true;
	}
	else token_installed = false;
	return token_installed;
}

void vkscript::VkScriptCore::clear()
{
	api.clear();
	token_link.clear();
	access_tokens.clear();
	authorize_uri.clear();
	access_token.clear();
	preselect_token.clear();
	user_name.clear();
	user_info.clear();
	old_start = 0;
	start_point = 0;
	end_point = 0;
	user_id = 0;
	c_clear();
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

bool vkscript::VkScriptCore::checkAccess()
{
	nlohmann::json jres = sendRequest("users.get");
	if (jres.find("error") != jres.end()) {
		addLog("Invalid access token. Try authentication again.", VK_ERROR);
		return false;
	}
	addLog("Access token is valid.");
	return true;
}

void vkscript::VkScriptCore::addLog(std::string str, int logtype)
{
	std::string lgt;
	if (logtype == VK_NORMAL) lgt = "[INFO] ";
	if (logtype == VK_ERROR) lgt = "[ERROR] ";
	if (logtype == VK_WARNING) lgt = "[WARNING] ";
	if (logtype == VK_SPECIAL) lgt = "[!] ";
	
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
	try { addLog({ "VkScriptCore finished in " + getElapsedTime(ztime) + '.' }); }catch (...){}
#ifdef VKS_LOGS
	try { saveLog(); } catch (...){}
#endif
}

std::string vkscript::getDate()
{
	char date[80];
	time_t seconds = time(nullptr);
	struct tm timeinfo;
	localtime_s(&timeinfo, &seconds);
	const char* format = "%d.%m.%y %H:%M:%S";
	strftime(date, 80, format, &timeinfo);
	return date;
}
std::string vkscript::getDate(ull unixtime)
{
	char date[80];
	time_t seconds(unixtime);
	struct tm timeinfo;
	localtime_s(&timeinfo, &seconds);
	const char* format = "%d %b %Y %H:%M:%S";
	strftime(date, 80, format, &timeinfo);
	return  date;
}

std::string vkscript::cleanString(const std::string& str)
{
	std::string clstring = str;
	clstring.erase(std::remove_if(clstring.begin(), clstring.end(), [](char& c)
		{
			std::string a = "abcdefghijklmnopqrstuvwxyz ";
			if (a.find(std::tolower(c)) == std::string::npos)
			{
				return true;
			}
			if (c == ' ')
			{
				c = '_';
			}
			c = std::tolower(c);
			return false;
		}), clstring.end());
	return clstring;
}

std::string vkscript::getElapsedTime(std::chrono::system_clock::time_point t0)
{
	const auto t = std::chrono::system_clock::now();
	const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(t - t0).count();
	
	std::string hh = "00", mm = "00" , ss;
	
	long long h = seconds / 3600;
	hh = std::to_string(h);
	
	long long m = (seconds - h * 3600) / 60;
	mm = std::to_string(m);
	
	long long s = seconds - h * 3600 - m * 60;
	ss = std::to_string(s);
	
	if (hh.size() < 2) hh.insert(0, 1, '0');
	mm.insert(0, 2 - mm.size(), '0');
	ss.insert(0, 2 - ss.size(), '0');
	
	return { hh + ':' + mm + ':' + ss };
}

double vkscript::simple_jaccard(const std::string& first_str, const std::string& second_str)
{
	std::string temp_f = first_str, temp_s = second_str;
	std::transform(temp_f.begin(), temp_f.end(), temp_f.begin(), [](unsigned char c) { return std::tolower(c); });
	std::transform(temp_s.begin(), temp_s.end(), temp_s.begin(), [](unsigned char c) { return std::tolower(c); });
	double a = temp_f.length();
	double b = temp_s.length();
	double c = 0;
	for (auto ch : temp_f) {
		auto ff = temp_s.find(ch);
		if (ff != std::string::npos)
		{
			temp_s.erase(temp_s.begin() + ff, temp_s.begin() + ff + 1);
			c++;
		}
	}
	return c / (a + b - c);
}
