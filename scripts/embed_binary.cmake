file(READ "${INPUT}" data HEX)
string(REGEX REPLACE "(..)" "0x\\1," bytes "${data}")
file(WRITE "${OUTPUT}" "${bytes}\n")
