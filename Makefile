xvisbell: xvisbell.cpp
	c++ -std=c++14 -Wall -Wextra -Werror -o xvisbell xvisbell.cpp -lX11
clean:
	rm xvisbell
install: xvisbell
	install xvisbell /usr/bin/
