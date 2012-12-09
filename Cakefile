exec = require('child_process').exec
task 'test', 'run all tests suites', ->
    console.log 'Running front-end tests'
    chrome_bin = "DISPLAY=:99"
    testacular = "#{__dirname}/node_modules/testacular/bin/testacular"
    browsers = '.travis/chrome-start.sh'
    options = "--single-run --browsers=#{browsers}"
    exec "#{chrome_bin} #{testacular} start #{__dirname}/.travis/testacular.conf.js", (err, stdout, stderr) ->
        console.error err if err
        console.log stdout