Description
-----------
This program is a multithreaded httpserver that performs `GET` and `PUT` operations that is thread safe and tracks the ordering of processes through an audit log.
`GET` operations retrieve contents of an existed file.
`PUT` operations creates (if file DNE) or opens edits the contents of a file.
NOTE - preferred processor to run on is ARM. x86 will work as well. 

Design
------
My design I chose to follow is modularizing every function from handling the connection, reading in from the socket, parsing the request, to setting all the appropriate struct variables. My goal in this assignment is mainly use regular expressions to match the request line with the method, uri, and http version. In order to make the program thread safe, I used mutexes and locks (flock) to cover race conditions and non-deterministic outputs.

How To Run
----------
In terminal 
   1. Move all files in the same directory.
   2. Open and run `make` in terminal.
   3. In the terminal, run `./httpserver <port_num>`
   4. Open up or split another terminal.
   5. To test, run `printf "<operation> /<file_name>.<file_extension>  HTTP/1.1\r\nContent-Length: <cl_num>\r\nRequest-Id: <req_id_num>\r\n\r\n" | nc localhost <port_num>`

   
Running & Testing
-----------------
In this implementation of HTTPSERVER, the only commands that are implemented are GET and SET.

- GET will return the specified file's content with the length of file's content and will return a status code of 200. 
- PUT will put or replace specified file's content with the content body that is only of length of the specified content length. It will either return 200 if the file exists and has replaced it, or return 201  if the file does not exist and has to be created. 

After opening/splitting another terminal, we must use a standardized format that is required in order to implement the HTTP commands.

- `printf "GET /<file> HTTP/1.1\r\n\r\n" | nc localhost <port>`
- `printf "PUT /<file> HTTP/1.1\r\nContent-Length: <content_length>\r\n<content_body>" | nc localhost <port>`

GET commands will only execute if there is `\r\n\r\n` at the end.
PUT commands always have to have a content length and content body.

Here are the status codes and their status phrases and description.
| Status Code | Status Phrase         | Description
| ----------- | ----------------------| -----------
| 200         | OK                    | Method was successful.
| 201         | Created               | URI's file is created.
| 400         | Bad Request           | Request is ill-formated.
| 403         | Forbidden             | URI's file is not accessible.
| 404         | Not Found             | URI's file does not exist with GET
| 500         | Internal Server Error | Issues reading in from socket
| 501         | Not Implemented       | Specified method that isn't part of HTTPSERVER
| 505         | Version Not Supported | HTTP/<version> is not supported. Has to be 1.1
