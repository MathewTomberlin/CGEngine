#pragma once

#include "../DataMap.h"

namespace CGEngine {
	class InputDataController {
	public:
		void setInput(DataMap in) {
			input = in;
		}

		DataMap getInput() {
			return input;
		}

		template<typename T>
		T getInputData(string key) {
			return input.getData<T>(key);
		}

		template<typename T>
		T* getInputDataPtr(string key) {
			return input.getDataPtr<T>(key);
		}

		void setInputData(string key, any value) {
			input.setData(key, value);
		}
	protected:
		DataMap input;
	};
}