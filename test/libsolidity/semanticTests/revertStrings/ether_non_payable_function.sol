contract C {
	function f() public {}
}
// ====
// EVMVersion: >=byzantium
// compileViaYul: also
// revertStrings: debug
// ----
// f(), 1 ether -> FAILURE, 3963877391197344453575983046348115674221700746820753546331534351508065746944, 862718293348820473429344482784628181556388621521298319395315527974912, 923952622198756772117572324632619918429858147389757005976759959578229, 49930134648677906276447105834838905336190317628633433094924853806487784390656, 26436264843829077938473849111408586943774010283420122633546943462400
// () -> FAILURE, 3963877391197344453575983046348115674221700746820753546331534351508065746944, 862718293348820473429344482784628181556388621521298319395315527974912, 0x35436f6e747261637420646f6573206e6f7420686176652066616c6c62, 44050003533197497889603156934101771025014898666797368672603377860743442464768, 26436264843829077938473849111408586943774010283420122633546943462400
