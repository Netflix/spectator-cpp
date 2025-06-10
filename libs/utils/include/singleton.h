#pragma once
// Templated Singleton for derived singleton classes

template <typename T> class Singleton
{
  protected:
    // Protected constructor & destructor allow derived classes to instantiate
    Singleton()          = default;
    virtual ~Singleton() = default;

  public:
    // Prevent copying
    Singleton(const Singleton &)            = delete;
    Singleton &operator=(const Singleton &) = delete;

    // Get the singleton instance
    static T &GetInstance()
    {
        static T instance;
        return instance;
    }
};
