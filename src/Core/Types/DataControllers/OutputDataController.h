#pragma once

#include "../DataMap.h"

namespace CGEngine {
	class OutputDataController {
	public:
		void setOutput(DataMap out) {
			output = out;
		}

		DataMap getOutput() {
			return output;
		}

		template<typename T>
		T getOutputData(string key) {
			return output.getData<T>(key);
		}

		template<typename T>
		T* getOutputDataPtr(string key) {
			return output.getDataPtr<T>(key);
		}

		void setOutputData(string key, any value) {
			output.setData(key, value);
		}
	protected:
		DataMap output;
	};
}