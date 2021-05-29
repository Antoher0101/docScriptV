#include "docScript.h"

vkscript::docScript::docScript() : begin_current_interval(0), end_current_interval(0), percent(0)
{
	searching = false;
}

bool vkscript::docScript::start()
{
	if (token_installed)
	{
		genIds();
		if (startSearching()) { return true; }
	}
	else addLog("docScript can not start. Access token is not installed.",VK_ERROR);
	return false;
}

void vkscript::docScript::stop()
{
	searching = false;
	addLog("Searching has been stopped.");
	saveInterruptedSearch(begin_current_interval);
}

bool vkscript::docScript::startSearching()
{
	if (is_user_found && token_installed)
	{
		percent = 0;
		searching = true;
		const double end = end_point;
		double interval;
		// Need refactor
		old_start == 0 ? interval = start_point - end_point : interval = old_start - end_point;
		addLog({ "Start searching... [" + std::to_string(start_point) + "-" + std::to_string(end_point) + "]" });

		for (auto& a : id_arrays)
		{
			if (!searching)
			{
				addLog("Failed to start search.", VK_ERROR);
				break;
			}
			end_current_interval = a.back();
			begin_current_interval = a.front();

			std::string p = createRequest(a);
			// Receiving a response from VK
			VK::json data = sendRequest("docs.getById", p);
			data["interval"] = std::to_string(begin_current_interval) + "-" + std::to_string(end_current_interval);
			
			if (data["error"].empty()) {
				if (!data["response"].empty()) {
					result += formatData(data, VK_EXTENDED);
					addLog({ "[" + std::to_string(begin_current_interval) + "-" + std::to_string(end_current_interval) + "] Something was found." }, VK_SPECIAL);
				}
			}
			else
			{
				std::string error = formatData(data, VK_ERROR, { "[" + std::to_string(begin_current_interval) + "-" + std::to_string(end_current_interval) + "] " });
				addLog(error, VK_ERROR);
				end_current_interval = begin_current_interval;
				saveInterruptedSearch(begin_current_interval);
				break;
			}
			
			double progress = 100 - round(((static_cast<double>(end_current_interval) - end) / interval) * 100.0);

			// Maybe I'll do some refactoring later
			if (progress == 100.0) progress = 99;
			if (percent < progress)
				addLog({ std::to_string(static_cast<int>(progress)) + "%" });
			// Rounding correction
			if (end_current_interval != end_point) {
				percent = progress;
			}
			else
			{
				percent = 100;
				addLog("100%");
				break;
			}
		}
		searching = false;
		// Updating the user's global scan progress
		updateScannedIntervals();
	}
	if (percent == 100) {
		addLog("Search completed.", VK_SPECIAL);
		saveResult();
		return true;
	}
	addLog("Search stopped.");
	saveResult();
	return false;
}

std::string vkscript::docScript::formatData(nlohmann::json& data, int mode, const std::string& prefix)
{
	std::string fdata;
	if (mode == VK_NORMAL)
	{
		nlohmann::basic_json<> inf = data["response"][0];
		std::string url = inf["url"].get<std::string>();
		fdata += "{\n\tTitle: " + inf["title"].get<std::string>() + "\n"
			+ "\tDate: " + getDate(inf["date"].get<unsigned long long>()) + "\n"
			+ "\tURL: " + url.erase(url.find('?'), std::string::npos) + "\n"
			+ "\tInterval: " + data["interval"].get<std::string>() + "\n"
			+ "}\n";
	}
	else if (mode == VK_ERROR)
	{
		nlohmann::basic_json<> err = data["error"];
		fdata += prefix
			+ "Error code: "
			+ err["error_code"].dump() + ". " // Warning! Is not a string
			+ err["error_msg"].get<std::string>();
	}
	else if (mode == VK_EXTENDED)
	{
		nlohmann::basic_json<> inf = data["response"][0];
		std::string url = inf["url"].get<std::string>();
		fdata += "{\n\tTitle: " + inf["title"].get<std::string>() + "\n"
			+ "\tOwner ID: " + inf["owner_id"].dump() + "\n"
			+ "\tID: " + inf["id"].dump() + "\n"
			+ "\tDate: " + getDate(inf["date"].get<unsigned long long>()) + "\n"
			+ "\tType: " + inf["ext"].get<std::string>() + "\n"
			+ "\tSize: " + inf["size"].dump() + "byte" + "\n"
			+ "\tURL: " + url.erase(url.find('?'), std::string::npos) + "\n"
			+ "\tInterval: " + data["interval"].get<std::string>() + "\n"
			+ "}\n";
	}
	return fdata;
}

void vkscript::docScript::updateScannedIntervals()
{
	if (scanned_intervals.empty()) { scanned_intervals.emplace_back(start_point, end_current_interval); }
	bool intersection(false);
	for (auto& pair : scanned_intervals)
	{

		if (checkIntersection(pair.first+1, pair.second-1, start_point, end_current_interval))
		{
			start_point > pair.first ? pair.first = start_point : pair.second = end_current_interval;
			intersection = true;
			break;
		}
		else intersection = false;
	}
	if (!intersection)
	{
		scanned_intervals.emplace_back(start_point, end_current_interval);
	}
}
/*Full data overwrite and user info update is performed*/
void vkscript::docScript::saveResult()
{
	addOtherUserInfo();
	std::string filename = "results/" + filename_ + ".txt";

	std::string dump;
	std::ifstream readdata(filename, std::ios::in);

	if (readdata.is_open() && readdata.peek() != EOF)
	{
		for (std::string str; std::getline(readdata, str);)
		{
			dump += str + '\n';
		}
		readdata.close();
	}
	// Delete old user info
	if (dump.find("User info") != std::string::npos)
	{
		size_t brace = dump.find('}', dump.find_first_of('}') + 1);
		auto uinf = std::find(dump.begin()+brace,dump.end(),'{');
		dump.erase(dump.begin(), uinf);
	}
	dump.insert(0, {user_info.dump(4) + '\n'});
	/*Save the updated user file*/
	std::ofstream outdata(filename, std::ios::trunc);
	if (outdata.is_open())
	{
		outdata << dump;
		outdata << result;
		outdata.close();
		addLog("Search results has been saved to a file.");
	}
}
void vkscript::docScript::readOtherUserInfo()
{
	std::vector<std::string> sc_in = user_info["User info"]["Scanned interval"].get<std::vector<std::string>>();
	for (auto &s : sc_in)
	{
		const auto first = stoull(s.substr(0, s.find('-')));
		const auto second = std::stoull(s.substr(s.find('-') + 1, std::string::npos));
		scanned_intervals.emplace_back(first, second);
	}
}

void vkscript::docScript::setFilename()
{
	filename_ = cleanString(user_name);
}

void vkscript::docScript::c_clear()
{
	id_arrays.clear();
	scanned_intervals.clear();
	begin_current_interval = 0;
	end_current_interval = 0;
	searching = false;
	percent = 0;
}

void vkscript::docScript::addOtherUserInfo()
{
	std::sort(scanned_intervals.begin(), scanned_intervals.end(), [](std::pair<ull,ull> left, std::pair<ull, ull> right)
		{
			return left.first > right.first;
		});
	// Checking for intersections of existing intervals and merge if true
	for (size_t i = 0; i < scanned_intervals.size()-1; i++)
	{
		if (checkIntersection(scanned_intervals[i], scanned_intervals[i+1]))
		{
			const std::pair<ull,ull> new_pair = std::make_pair(scanned_intervals[i].first, scanned_intervals[i + 1].second);
			scanned_intervals.erase(scanned_intervals.begin()+i+1);
			scanned_intervals[i] = new_pair;
			i--;
		}
	}
	std::vector<std::string> new_intervals;
	for (auto pair : scanned_intervals)
	{
		new_intervals.push_back(std::to_string(pair.first) + '-' + std::to_string(pair.second));
	}
	user_info["User info"]["Scanned interval"].clear();
	user_info["User info"]["Scanned interval"] = new_intervals;
}

std::string vkscript::docScript::createRequest(std::vector<ull> params)
{
	std::string ids = "docs=";
	const std::string ui = std::to_string(user_id);
	for (auto id : params)
	{
		ids += ui + "_" + std::to_string(id) + ",";
	}
	ids.pop_back();
	return ids;
}

void vkscript::docScript::loadBackup(const char* path)
{
	std::stringstream save_data;
	std::ifstream bkp(path, std::ios_base::in);
	if (bkp.is_open())
	{
		save_data << bkp.rdbuf();
		bkp.close();
		addLog("Backup has been loaded.");
	}
	else
	{
		addLog({"No backup found on " + std::string(path) + '.'}, VK_WARNING);
	}
	if (save_data.rdbuf()->in_avail()!= 0)
	{
		nlohmann::json j = nlohmann::json::parse(save_data);
		old_start = j["start_point"].get<unsigned long long>();
		setUser(std::to_string(j["user_id"].get<unsigned long long>()));
		setStartPoint(j["breakpoint"].get<unsigned long long>());
		setEndPoint(j["end_point"].get<unsigned long long>());
	}
}

void vkscript::docScript::genIds()
{
	addLog("Creating an array of search IDs...");
	if (start_point > end_point)
	{
		const unsigned long long search_interval = start_point - end_point;
		const int size_of_one_request = VK_SIZE_OF_ONE_REQUEST;

		for (ull i = static_cast<unsigned int>(ceil(static_cast<double>(search_interval) / static_cast<double>(size_of_one_request))),
			cur_id = start_point; i > 0; i--)
		{
			std::vector<ull> id_set;
			for (int j = size_of_one_request; j > 0 && cur_id >= end_point; j--, cur_id--)
			{
				id_set.emplace_back(cur_id);
			}
			id_arrays.push_back(id_set);
		}
		addLog("Array creation completed successfully.");
	}
	else if (start_point == end_point)
	{
		id_arrays.push_back({ start_point });
	}
	else {
		addLog("Wrong search interval.", VK_ERROR);
		exit(0);
	}
}


void vkscript::docScript::saveInterruptedSearch(ull breakpoint)
{
	nlohmann::json bkp{};
	old_start != 0 ? bkp["start_point"] = old_start : bkp["start_point"] = start_point;
	bkp["end_point"] = end_point;
	bkp["user_id"] = user_id;
	bkp["breakpoint"] = breakpoint;

	std::string filename = "backups//" + filename_ + "_backup" + ".json";

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

void vkscript::docScript::setStartPoint(ull n)
{
	start_point = n;
	addLog({ "Start point " + std::to_string(n) + " has been set." });
}

void vkscript::docScript::setEndPoint(ull n)
{
	end_point = n;
	addLog({ "End point " + std::to_string(n) + " has been set." });
}

bool vkscript::docScript::getSearchStatus() const
{
	return searching;
}

nlohmann::json vkscript::docScript::getResult() const
{
	return result;
}

std::string vkscript::docScript::getFilename() const
{
	return filename_;
}

double vkscript::docScript::getPercent() const
{
	return percent;
}

bool vkscript::checkIntersection(ull a1, ull a2, ull b1, ull b2)
{
	if (a1 == b1 || a1 == b2 || a2 == b1 || a2 == b2) return true;

	ull a1_ = a1, a2_ = a2, b1_ = b1, b2_ = b2;
	
	if (a1_ < a2_) { std::swap(a1_, a2_); }
	if (b1_ < b2_) { std::swap(b1_, b2_); }
	return std::max(a2_, b2_) <= std::min(a1_, b1_);
}
bool vkscript::checkIntersection(const std::pair<ull, ull> a, const std::pair<ull, ull> b)
{
	if (a.first == b.first || a.first == b.second || a.second == b.first || a.second == b.second) return true;
	ull a1_ = a.first, a2_ = a.second, b1_ = b.first, b2_ = b.second;

	if (a1_ < a2_) { std::swap(a1_, a2_); }
	if (b1_ < b2_) { std::swap(b1_, b2_); }
	return std::max(a2_, b2_) <= std::min(a1_, b1_);
}
