/*
    form.tst - Test forms
 */

load("support.es")

//  Form data
data = run("--form 'name=John+Smith&address=300+Park+Avenue' /form.ejs")
assert(data.contains('"address": "300 Park Avenue"'))
assert(data.contains('"name": "John Smith"'))

//  Form data with a cookie
data = run("--cookie 'test-id=12341234; $domain=site.com; $path=/dir/' /form.ejs")
assert(data.contains('"test-id": '))
assert(data.contains('"name": "test-id",'))
assert(data.contains('"domain": "site.com",'))
assert(data.contains('"path": "/dir/",'))
