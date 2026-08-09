module;

export module engine.renderer.token_guard;

import engine.core;

export namespace engine
{

  // Tracks whether an ID handed out by a begin_*() call (FrameID/CopyPassID/
  // PassID) is still the current, open one.
  template <typename ID>
  class TokenGuard
  {
  public:
    // Starts a new token, marks it open, and returns it as the ID to hand
    // back to the caller.
    [[nodiscard]] auto begin() -> ID
    {
      ++token_;
      open_ = true;
      return static_cast<ID>(token_);
    }

    // Marks the current token closed without changing it, so a stale ID
    // still reads as stale afterward instead of becoming valid again.
    auto end() -> void { open_ = false; }

    // For call sites that need to know a pass is still open without having
    // its ID at hand (e.g. submit() refusing to run with a pass left open).
    [[nodiscard]] auto is_open() const -> bool { return open_; }

    // Reports Severity::Error ("<what> id does not match the current
    // <what>") and returns false if id isn't the current, open token;
    // returns true otherwise.
    [[nodiscard]] auto check(ID id, App& app, const char* what) -> bool
    {
      if (id == ID::Invalid || static_cast<u64>(id) != token_ || !open_)
      {
        app.report(Severity::Error, "{} id does not match the current {}", what, what);
        return false;
      }
      return true;
    }

  private:
    u64 token_ = 0;
    bool open_ = false;
  };

} // namespace engine
