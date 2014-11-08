PLost
=====

PLost is like ping excpet that by using a custom server, it can tell you
whether the packets were lost on their way to the server or on their way
back.


Building
--------

All you should have to do is type `make`.


Usage
-----

Fairly simple. On one machine, run the server (PLostd). On the other
machine, run the client passing the server's name or IP as an argument:
`./PLost server.example.com`.
