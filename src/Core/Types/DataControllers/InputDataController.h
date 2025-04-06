#pragma once

#include "../DataMap.h"

namespace CGEngine {
	/// <summary>
	/// Data interface for input DataMap with arbitrary data types.
	/// </summary>
	class InputDataController {
	public:
		/// <summary>
		/// Overwrite input data with the given DataMap.
		/// </summary>
		/// <param name="in">The input DataMap</param>
		void setInput(DataMap in) {
			input = in;
		}

		/// <summary>
		/// Return the input DataMap.
		/// </summary>
		/// <returns>The input DataMap</returns>
		DataMap getInput() {
			return input;
		}

		/// <summary>
		/// Find the input data of the given T type and key and return it.
		/// </summary>
		/// <typeparam name="T">The expected type of input data to retrieve</typeparam>
		/// <param name="key">The key of input data to retrieve</param>
		/// <returns>The input data or the T type default value</returns>
		template<typename T>
		T getInputData(string key) {
			return input.getData<T>(key);
		}

		/// <summary>
		/// Find the input data pointer of the given T type and key and return it.
		/// </summary>
		/// <typeparam name="T">The expected type of the input data pointer to retrieve</typeparam>
		/// <param name="key">The key of the input data pointer to retrieve</param>
		/// <returns>The input data pointer or nullptr if not found</returns>
		template<typename T>
		T* getInputDataPtr(string key) {
			return input.getDataPtr<T>(key);
		}

		/// <summary>
		/// Set the input data at key to the given value
		/// </summary>
		/// <param name="key">The key to add the value to</param>
		/// <param name="value">The value to add at key</param>
		void setInputData(string key, any value) {
			input.setData(key, value);
		}
	protected:
		DataMap input;
	};
}