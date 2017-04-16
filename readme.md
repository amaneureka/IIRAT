# IIRAT
An application that will let you control your smart devices from remote. Currently supported only on windows (xp or later) OS.

#### Supported Platforms
- **Windows**
- **Linux (_Coming Soon_)**

#### What is the exact meaning of "control your smart devices" ?
- **shell access**
- **screen capture**
- **keyboard hook**
- **filesystem**

#### Inspiration
One of my friend challenged me to create such an application.

#### Is it reliable?
**Yes!**, IIRAT has it's own complete bundle of server and client code. It does not rely on any 3rd party software. Each device register itself on server for the first time it gets connected. And from their onwards it uses it's own UID to send and recieve messages.

#### How it exactly work?
_Good_ question. Here is the timeline of IIRAT
- IIRAT gets installed
    - Creates a Local service.
    - Creates an application copy.
- Ping Server
- Server Accepts request and send Identity
- IIRAT client sends request for registration
- Server register client and send UID
- IIRAT accepts UID and save it.
- IIRAT send Login request with UID
- Server accepts and send Hello
- Done!

So, How can I control and send command to my IIRAT?
Use Client.py and connect with admin UID. only Admin has permissions to send request to other clients. And there is only one Admin. But It can have multiple sessions.

> **```CMD<device_id><type><command>```**
>    - device_id : IIRAT device_id
>    - type: ```SSCR``` for screenshot and ```EXEC``` for shell command.
>    - command : Any shell command if type if ```EXEC```

#### Screenshot
- Server

![Server](http://i.imgur.com/vi4XyVP.png)

- Client

![Client](http://i.imgur.com/qzOyqvV.png)

#### How can I contribute?
Feel free to ping me on email.

#### License
&copy; [Aman Priyadarshi](https://twitter.com/amaneureka)
