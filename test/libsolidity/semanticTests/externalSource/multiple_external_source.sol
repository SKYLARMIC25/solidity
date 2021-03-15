==== ExternalSource: _external/otherExternal.sol ====
==== ExternalSource: _external/external.sol ====
import "_external/external.sol";
import "_external/otherExternal.sol";
contract C {
  External _external;
  OtherExternal _otherExternal;
  constructor() {
    _external = new External();
    _otherExternal = new OtherExternal();
  }
  function foo() public returns (uint) {
    return _external.foo();
  }
  function foofoo() public returns (uint) {
    return _otherExternal.foo();
  }
}
// ====
// compileViaYul: also
// ----
// foo() -> 1234
// foofoo() -> 2345
