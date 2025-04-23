template<typename T>
class StateFlow {
    T value;
    bool changed = false;

public:
    StateFlow(T initial) : value(initial) {}

    // Conditional state update
    template<typename Cond, typename Func>
    StateFlow& thenIf(Cond condition, Func action) {
        if (!changed && condition()) {
            value = action();
            changed = true;
        }
        return *this;
    }
    
    template<typename Func>
    StateFlow& otherwise(Func action) {
        if (!changed) {
            value = action();
            changed = true;
        }
        return *this;
    }

    template<typename Cond, typename Func>
    StateFlow& doIf(Cond condition, Func action) {
        if (!changed && condition()) action();
        return *this;
    }

    // Get final value
    T get() const { return value; }
};
