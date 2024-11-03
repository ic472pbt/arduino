class IState{
 protected:
    String stateName;  // Protected so derived classes can set it
    
 public: 
  // Constructor to set the state name
  IState(const String& name) : stateName(name) {}

  virtual ~IState() = default;
  virtual IState* Handle(float PVvoltage, unsigned long currentTime) = 0; 
  // Get the name of the state
  const String& GetName() const {
    return stateName;
  }
};


class floatState : public IState {
public:
    char dirrection;  // Public property for accessing flip
    void Reverse() {dirrection *= -1;}
    
    floatState() : IState("float"), dirrection(1) {}
    IState* Handle(float PVvoltage, unsigned long currentTime) override;
};
