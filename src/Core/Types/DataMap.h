#pragma once

#include <any>
#include <optional>
#include <map>

namespace CGEngine {
	/// <summary>
	/// Stores data of arbitrary type by string key. Data is stored as any type, so its type must be known
	/// when retrieved to decode it. DataMap is used to store input and output data for Scripts and Behaviors.
	/// </summary>
	class DataMap {
	public:
		/// <summary>
		/// Construct and initialize the DataMap with the given map of string keys to any values.
		/// </summary>
		/// <param name="data">The data to initialize the DataMap with</param>
		DataMap(map<string, any> data = {}) : data(data) {};

		/// <summary>
		/// Set the data at the given key to the given value. If the key already exists, it is overwritten.
		/// </summary>
		/// <param name="key">The string key of the data</param>
		/// <param name="val">The value of the data to set</param>
		void setData(string key, any val) {
			data[key] = val;
		}

		/// <summary>
		/// Remove the data at the given key. If the key does not exist, nothing happens.
		/// </summary>
		/// <param name="key">The string key of the data to remove</param>
		void removeData(string key) {
			if (data.find(key) != data.end()) {
				data.erase(key);
			}
		}

		/// <summary>
		/// Return the data at the given key. If the key does not exist, nullopt is returned.
		/// </summary>
		/// <param name="key">The string key of the data to return</param>
		/// <returns>The data at the key or nullopt if not found</returns>
		optional<any> getData(string key) {
			if (data.find(key) != data.end()) {
				return data[key];
			}
			return nullopt;
		}

		/// <summary>
		/// Return the data at the given key, cast to T type. If the key does not exist, default value of T type is returned. If the type is not correct, an exception is thrown.
		/// </summary>
		/// <typeparam name="T">The type to cast the found data to</typeparam>
		/// <param name="key">The string key of the data to return</param>
		/// <returns>The data at the key, cast to T type, or default value of T type if not found. Throws an exception if type is incorrect.</returns>
		template<typename T>
		T getData(string key) {
			static T test;
			if (data.find(key) != data.end()) {
				return any_cast<T>(data[key]);
			}
			return test;
		}

		/// <summary>
		/// Return the data pointer at the given key, cast to T type. If the key does not exist, nullptr is returned. If the type is not correct, an exception is thrown.
		/// </summary>
		/// <typeparam name="T">The type to cast the found data pointer to</typeparam>
		/// <param name="key">The string key of the data pointer to return</param>
		/// <returns>The data pointer at the key, cast to T type, or nullptr if not found. Throws an exception if type is incorrect.</returns>
		template<typename T>
		T* getDataPtr(string key) {
			if (data.find(key) != data.end()) {
				return any_cast<T*>(data[key]);
			}
			return nullptr;
		}

		/// <summary>
		/// Return the data at the given key, then erase it from the data map. If the key does not exist, nullopt is returned.
		/// </summary>
		/// <param name="key">The string key of the data to return</param>
		/// <returns>The data at the key or nullopt if not found</returns>
		optional<any> pullOutData(string key) {
			if (data.find(key) != data.end()) {
				any d = data[key];
				data.erase(key);
				return d;
			}
			return nullopt;
		}

		/// <summary>
		/// Return the data at the given key, cast to T type, then erase it from the data map. If the key does not exist, default value of T type is returned.
		/// If the type is not correct, an exception is thrown.
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="key">The string key of the data to return</param>
		/// <returns>The data, cast to T type, at the key or default value of T type if not found</returns>
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
		/// <summary>
		/// Map of string key to any type value
		/// </summary>
		map<string, any> data;
	};
}