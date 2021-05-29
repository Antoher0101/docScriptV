#pragma once
#include "VkScriptCore.h"
#include "VK/src/api.h"
#include "VK/src/api.h"

namespace vkscript
{
	class docScript : public VkScriptCore
	{
		std::string result;
		std::string filename_;
		//First ID in the array
		ull begin_current_interval;
		//Last ID in the array
		ull end_current_interval;
		
		std::vector<std::pair<ull, ull>> scanned_intervals;
		bool searching;
		double percent;
		
		std::vector<std::vector<ull>> id_arrays;

		void saveInterruptedSearch(ull breakpoint);
		bool startSearching();
		void genIds();
		void addOtherUserInfo() override;
		void readOtherUserInfo() override;
		void setFilename() override;
		void c_clear() override;
		std::string createRequest(std::vector<ull> params);
		std::string formatData(nlohmann::json& data, int mode = VK_NORMAL, const std::string& prefix = {}) override;
	public:
		docScript();

		bool start() override;
		void stop() override;
		void updateScannedIntervals();
		nlohmann::json getResult() const override;
		std::string getFilename()const override;
		void saveResult() override;
		
		void loadBackup(const char* path);
		
		bool getSearchStatus() const;
		void setStartPoint(ull n);
		void setEndPoint(ull n);

		double getPercent()const;
	};
	// Utils

	bool checkIntersection(ull a1,ull a2,ull b1,ull b2);
	bool checkIntersection(std::pair<ull, ull> a, std::pair<ull, ull> b);
}
