#pragma once

#include <any>
#include <stack>
#include <optional>
#include <functional>
using namespace std;

namespace CGEngine {
	class DataStack {
	public:
		DataStack(stack<any> data = {});

		bool hasData() const;
		void push(any val);
		void pop();
		optional<any> peek() const;
		template <typename T>
		optional<T> peek() const {
			if (dataStack.empty()) return nullopt;
			return any_cast<T>(dataStack.top());
		}
		optional<any> pullOut();
		template <typename T>
		optional<T> pullOut() {
			if (dataStack.empty()) return nullopt;
			T data = any_cast<T>(dataStack.top());
			dataStack.pop();
			return data;
		}
		void giveTo(DataStack* target) const;
		void clear();

		/// <summary>
		/// Iterate through data values, casting them to the provided type and applying the provided function on each element. Ignore values of other types and
		/// leave them in the dataStack
		/// </summary>
		/// <typeparam name="T">The type to cast to</typeparam
		/// <param name="func">The function to apply to each element</param>
		template <typename T>
		void forEach(function<void(T)> func) const {
			//If the returnStack is populated
			if (hasData()) {
				//Get a copy of the returnStack
				stack<any> _stack = dataStack;
				stack<any> remaining;
				while (!_stack.empty()) {
					//Get the top val of the stack
					any val = _stack.top();
					//If its type == Cast type
					if (val.type() == typeid(T)) {
						//Cast and call the event
						T x = any_cast<T>(val);
						func(x);
						//If its type != Cast type
					}
					else {
						//Push it to the stack of remaining vals
						remaining.push(val);
					}
					//Pop it from stack
					_stack.pop();
				}
				//Replace returnStack with the remaining items
				dataStack = remaining;
			}
		};
	private:
		stack<any> dataStack;
	};
}