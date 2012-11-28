/*
JavaScript for the demo: Recreating the Nikebetterworld.com Parallax Demo
Demo: Recreating the Nikebetterworld.com Parallax Demo
Author: Ian Lunn
Author URL: http://www.ianlunn.co.uk/
Demo URL: http://www.ianlunn.co.uk/demos/recreate-nikebetterworld-parallax/
Tutorial URL: http://www.ianlunn.co.uk/blog/code-tutorials/recreate-nikebetterworld-parallax/

License: http://creativecommons.org/licenses/by-sa/3.0/ (Attribution Share Alike). Please attribute work to Ian Lunn simply by leaving these comments in the source code or if you'd prefer, place a link on your website to http://www.ianlunn.co.uk/.

Dual licensed under the MIT and GPL licenses:
http://www.opensource.org/licenses/mit-license.php
http://www.gnu.org/licenses/gpl.html
*/
$(document).ready(function(){
    $('#nav').localScroll(800);
    window.scrollTo(0, 1);
    $(window).resize(function () {
        $('.window-div').css({"height": $(window).height() + "px"});
        $('body').css({"height": 4*$(window).height() + "px"});
    });
    $('.window-div').css({"height": $(window).height() + "px"});
    $('body').css({"height": 4*$(window).height() + "px"});

    var ua = navigator.userAgent;
    var checker = {
      iphone: ua.match(/(iPhone|iPod|iPad)/),
      blackberry: ua.match(/BlackBerry/),
      android: ua.match(/Android/)
    };

    if (checker.android ||checker.iphone ||checker.blackberry) {

    } else {

    //$('#intro').parallax("40%", "90%", 0, -0.05, 0);
    //$('.logo').parallax("40%", "40%", 1, 1, 0);
    //$('.bg1').parallax("0%", "0%", 1.5, 1, 0);
    //$('.imglogo1').parallax("80%", "40%", 0, 0.1, 0);
    //$('.imglogo2').parallax("80%", "50%", 0, 0.2, 3);
    //$('.imglogo3').parallax("80%", "50%", 0, 0.40, 3);
    //$('.imglogo4').parallax("80%", "50%", 0, 0.60, 3);
    //$('#second').parallax("40%", "90%", 0, -0.05, 0);
    //$('#third').parallax("40%", "90%", 0, -0.05, 0);
    //$('.bg').parallax("40%", "50%", 0, 3, 0);
    $('.webcam').parallax("10%", "50%", 0, 1, 0);
    $('.html5').parallax("80%", "50%", 0, 1, 0);
    //$('.d').parallax("70%", "60%", -0.1, .3, 0);
    //$('.i').parallax("85%", "60%", -0.5, .3, 0);
    //$('.y').parallax("100%", "60%", -1, .3, 0);
    //$('#fifth').parallax("40%", "90%", 0, -0.05, 0);
    //$('#third').parallax("50%", "0", 0.3, 0.6, 0, vertical);
    }

})