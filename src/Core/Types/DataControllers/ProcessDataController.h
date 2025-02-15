#pragma once

#include "../DataMap.h"

namespace CGEngine {
	class ProcessDataController {
	public:
		void setProcess(DataMap data) {
			process = data;
		}

		DataMap getProcess() {
			return process;
		}

		template<typename T>
		T getProcessData(string key) {
			return process.getData<T>(key);
		}

		template<typename T>
		T* getProcessDataPtr(string key) {
			return process.getDataPtr<T>(key);
		}

		void setProcessData(string key, any value) {
			process.setData(key, value);
		}
	protected:
		DataMap process;
	};
}