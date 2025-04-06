#pragma once

#include "../DataMap.h"

namespace CGEngine {
	/// <summary>
	/// Data interface for process DataMap with arbitrary data types.
	/// </summary>
	class ProcessDataController {
	public:
		/// <summary>
		/// Overwrite process data with the given DataMap.
		/// </summary>
		/// <param name="data">The process DataMap</param>
		void setProcess(DataMap data) {
			process = data;
		}

		/// <summary>
		/// Return the process DataMap.
		/// </summary>
		/// <returns>The process DataMap</returns>
		DataMap getProcess() {
			return process;
		}

		/// <summary>
		/// Find the process data of the given T type and key and return it.
		/// </summary>
		/// <typeparam name="T">The expected type of process data to retrieve</typeparam>
		/// <param name="key">The key of process data to retrieve</param>
		/// <returns>The process data or the T type default value</returns>
		template<typename T>
		T getProcessData(string key) {
			return process.getData<T>(key);
		}

		/// <summary>
		/// Find the process data pointer of the given T type and key and return it.
		/// </summary>
		/// <typeparam name="T">The expected type of the process data pointer to retrieve</typeparam>
		/// <param name="key">The key of the process data pointer to retrieve</param>
		/// <returns>The process data pointer or nullptr if not found</returns>
		template<typename T>
		T* getProcessDataPtr(string key) {
			return process.getDataPtr<T>(key);
		}

		/// <summary>
		/// Set the process data at key to the given value
		/// </summary>
		/// <param name="key">The key to add the value to</param>
		/// <param name="value">The value to add at key</param>
		void setProcessData(string key, any value) {
			process.setData(key, value);
		}
	protected:
		DataMap process;
	};
}