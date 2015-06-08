--- 
layout: post 
title: Introducing custom Cloud Handler policies
--- 

As you know Licode is designed to be deployed in private or public clouds taking advantage of the scalability capabilities of this type of environments. One of the scalability levels that Licode offers is the possibility of having more than one Erizo Controller conected to the same Nuve. Thus, in the same service you can have different Erizo Controllers managing different rooms. When a user connects to a new Room, Nuve decides to which Erizo Controller assign the room depending on different conditions. 

The component that takes that decision is called Cloud Handler and until know it took the decision basing on the number of rooms each Erizo Controller was managing. Today we introduce a machanism to customize that decision thus enabling a more advanced resource handling. 

And it is so easy, you can start from the template we have created as the [default policy script](https://github.com/ging/licode/blob/master/nuve/nuveAPI/ch_policies/default_policy.js) and modify it introducing your own algorithm. The algorithm has to be encapsulated in a method with the following format: 


	Params:
	---
	room: room to which we need to assing a erizoController.
		{
		name: String, 
		[p2p: bool], 
		[data: Object], 
		_id: ObjectId
		}
	ec_list: array with erizo controller objects
		
		{
        ip: String,
        rpcID: String,
        state: Int,
        keepAlive: Int,
        hostname: String,
        port: Int,
        ssl: bool
    	}
    ec_queue: array with erizo controllers priority according rooms load

	Returns:
	---
	erizoControlerId: the key of the erizo controller selected from ec_list


Once implemented, you have only to set it in your <code>licode_config.js</code> file:

	// Cloud Handler policies are in nuve/nuveAPI/ch_policies/ folder
	config.nuve.cloudHandlerPolicy = 'my_own_policy.js'; // default value: 'default_policy.js'
