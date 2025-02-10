#include "DataStack.h"

namespace CGEngine {
	DataStack::DataStack(stack<any> data) {
		dataStack = data;
	}

	void DataStack::push(any val) {
		dataStack.push(val);
	}

	optional<any> DataStack::peek() const {
		if (dataStack.empty()) return nullopt;
		any val = dataStack.top();
		return val;
	}

	optional<any> DataStack::pullOut() {
		if (dataStack.empty()) return nullopt;
		any val = dataStack.top();
		dataStack.pop();
		return val;
	}

	void DataStack::pop() {
		dataStack.pop();
	}

	void DataStack::giveTo(DataStack* target) const {
		target->dataStack = dataStack;
	}

	void DataStack::clear() {
		dataStack = stack<any>();
	}

	bool DataStack::hasData() const {
		return !dataStack.empty();
	}
}