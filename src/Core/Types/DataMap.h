#pragma once

#include <any>
#include <optional>
#include <map>
using namespace std;

namespace CGEngine {
	class DataMap {
	public:
		DataMap(map<string, any> d = {}) : data(d) {};

		void setData(string key, any val) {
			data[key] = val;
		}

		void removeData(string key) {
			if (data.find(key) != data.end()) {
				data.erase(key);
			}
		}

		optional<any> getData(string key) {
			if (data.find(key) != data.end()) {
				return data[key];
			}
			return nullopt;
		}

		template<typename T>
		T getData(string key) {
			static T test;
			if (data.find(key) != data.end()) {
				return any_cast<T>(data[key]);
			}
			return test;
		}

		template<typename T>
		T* getDataPtr(string key) {
			if (data.find(key) != data.end()) {
				return any_cast<T*>(data[key]);
			}
			return nullptr;
		}

		optional<any> pullOutData(string key) {
			if (data.find(key) != data.end()) {
				any d = data[key];
				data.erase(key);
				return d;
			}
			return nullopt;
		}

		template<typename T>
		T pullOutData(string key) {
			static T test;
			if (data.find(key) != data.end()) {
				T d = any_cast<T>(data[key]);
				data.erase(key);
				return d;
			}
			return test;
		}
	private:
		map<string, any> data;
	};
}