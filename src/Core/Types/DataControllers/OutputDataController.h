#pragma once

#include "../DataMap.h"

namespace CGEngine {
	/// <summary>
	/// Data interface for output DataMap with arbitrary data types.
	/// </summary>
	class OutputDataController {
	public:
		/// <summary>
		/// Overwrite output data with the given DataMap.
		/// </summary>
		/// <param name="out">The output DataMap</param>
		void setOutput(DataMap out) {
			output = out;
		}

		/// <summary>
		/// Return the output DataMap.
		/// </summary>
		/// <returns>The output DataMap</returns>
		DataMap getOutput() {
			return output;
		}

		/// <summary>
		/// Find the output data of the given T type and key and return it.
		/// </summary>
		/// <typeparam name="T">The expected type of output data to retrieve</typeparam>
		/// <param name="key">The key of output data to retrieve</param>
		/// <returns>The output data or the T type default value</returns>
		template<typename T>
		T getOutputData(string key) {
			return output.getData<T>(key);
		}

		/// <summary>
		/// Find the output data pointer of the given T type and key and return it.
		/// </summary>
		/// <typeparam name="T">The expected type of the output data pointer to retrieve</typeparam>
		/// <param name="key">The key of the output data pointer to retrieve</param>
		/// <returns>The output data pointer or nullptr if not found</returns>
		template<typename T>
		T* getOutputDataPtr(string key) {
			return output.getDataPtr<T>(key);
		}

		/// <summary>
		/// Set the output data at key to the given value
		/// </summary>
		/// <param name="key">The key to add the value to</param>
		/// <param name="value">The value to add at key</param>
		void setOutputData(string key, any value) {
			output.setData(key, value);
		}
	protected:
		DataMap output;
	};
}