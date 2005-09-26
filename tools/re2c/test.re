/*!re2c
digits		= [0-9];

(digits+ | digits* "." digits+) ([dDeE] [+-]? digits+)? { return true; }
[\000-\377] { return false; }
*/