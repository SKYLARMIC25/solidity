==== ExternalSource: _external/import_with_subdir.sol ====
==== ExternalSource: subExternal.sol=_external/subdir/subExternal.sol ====
import "_external/subdir/import.sol";
contract C {
  SubExternal _external;
  constructor() {
    _external = new SubExternal();
  }
  function foo() public returns (uint) {
    return _external.foo();
  }
}
// ====
// compileViaYul: also
// ----
// foo() -> 0x0929
