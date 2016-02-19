--- 
layout: post 
title: Multiple Ice Servers Configuration
--- 

Till now, Licode configuration file allows only to configure a single Stun server and a single Turn server. We have just published an update that allows you to introduce multiple Ice servers. The new configuration parameter <code>iceServers</code> is an array in wich you can set several servers independently of the type:


Use undefined to run clients without Ice Servers

```javascript
Stun servers format
{
    "url": url
}

Turn servers format
{
    "username": username,
    "credential": password,
    "url": url
}
```

For instance:

```javascript
config.erizoController.iceServers = [
	{
		'url': 'stun:stun.l.google.com:19302'
	},
	{
		'url': 'stun:stun.mystunserver.com:19302'
	},
	{
		'username': 'turn_username',
		'credential': 'turn_password',
		'url: 'turn:myturnserver.com:80?transport=tcp'
	}
];
```