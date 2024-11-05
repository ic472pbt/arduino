#ifndef IIRFILTER_H
#define IIRFILTER_H

class IIRFilter {
private:
    unsigned int alpha;
    unsigned int Q;
    unsigned int oldValue;

public:
    // Constructor to initialize alpha, Q, and set initial oldValue to 0
    IIRFilter(unsigned int alphaInit, unsigned int QInit)
        : alpha(alphaInit), Q(QInit), oldValue(0) {}

    // Method to reset the oldValue to a specified value (optional parameter, default is 0)
    void reset(unsigned int initialValue = 0) {
        oldValue = initialValue;
    }

    // Method to apply IIR filtering and update oldValue with smoothed value
    unsigned int smooth(unsigned int newValue) {
        // Apply IIR formula
        oldValue = (unsigned int)((alpha * (unsigned long)(oldValue) + (Q - alpha) * (unsigned long)(newValue)) / Q);
        return oldValue;
    }

    // Optional: Method to get the current oldValue without modifying it
    unsigned int getOldValue() const {
        return oldValue;
    }
};
#endif // IIRFILTER_H
