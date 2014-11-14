---
layout: post
title: Configuration of roles in Licode (update)
---

This is an update of the previous [post](http://lynckia.com/licode/roles.html) about roles management in Licode.

We have changed the format in which roles are defined in Licode's installation. The new format is:

```javascript
config.roles = {
  "teacher": {"publish": true, "subscribe": true, "record": true},
  "student": {"subscribe": true}
};
```
We can edit the roles in the configuration file that we can find at `licode/licode_config.js` once installed.
