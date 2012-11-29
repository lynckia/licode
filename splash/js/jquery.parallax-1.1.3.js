/*
Plugin: jQuery Parallax
Version 1.1.3
Author: Ian Lunn
Twitter: @IanLunn
Author URL: http://www.ianlunn.co.uk/
Plugin URL: http://www.ianlunn.co.uk/plugins/jquery-parallax/

Dual licensed under the MIT and GPL licenses:
http://www.opensource.org/licenses/mit-license.php
http://www.gnu.org/licenses/gpl.html
*/

(function( $ ){
	var $window = $(window);
	var windowHeight = $window.height();
	var windowWidth = $window.width();

	$window.resize(function () {
		windowHeight = $window.height();
		windowWidtht = $window.width();
	});

	$.fn.parallax = function(xpos, ypos, xspeedFactor, yspeedFactor, speedOpacity, callback, outerWidth, outerHeight) {

		var $this = $(this);
		$this.css('left', xpos);
		var getHeight;
		var firstTop;
		var firstLeft;
		var paddingTop = 0;
		
		//get the starting position of each element to have parallax applied to it
		$this.each(function(){
		    firstTop = $this.offset().top;
		    firstLeft = $this.offset().left;
		});

		$this.css('left', 0);

		if (outerHeight) {
			getHeight = function(jqo) {
				return jqo.outerHeight(true);
			};
            
            getWidth = function(jqo) {
				return jqo.outerWidth(true);
			};
		} else {
			getHeight = function(jqo) {
				return jqo.height();
			};
            
            getWidth = function(jqo) {
				return jqo.width();
			};
		}
			
		// setup defaults if arguments aren't specified
		//if (arguments.length < 1 || xpos === null) xpos = "50%";
		//if (arguments.length < 2 || ypos === null) ypos = "50%";
		//if (arguments.length < 3 || xspeedFactor === null) xspeedFactor = 0.1;
        //if (arguments.length < 4 || yspeedFactor === null) yspeedFactor = 0.1;
		//if (arguments.length < 5 || outerHeight === null) outerHeight = true;
        //if (arguments.length < 6 || outerWidth === null) outerWidth = true;

		function changeOpacity(pos, speedOpacity){
			var newOpacity = 1 - (pos/950)*speedOpacity;
			return newOpacity;
		}
        
        function movement(xpos, ypos, firstLeft, firstTop, pos, xSpeedFactor, ySpeedFactor, superWidth, superHeight) {
        	//console.log("pos " + pos);
        	//console.log("ypos " + ypos);
        	//console.log("result " + (ypos.replace("%","") - (pos-firstTop) * ySpeedFactor));
            return (xpos.replace("%","") - (pos-firstTop) * xSpeedFactor + "% ") + " " + (ypos.replace("%","") - (pos-firstTop) * ySpeedFactor) + "% ";
        }
		
		// function to be called whenever the window is scrolled or resized
		function update(e, offsetY){
			var pos = offsetY || $window.scrollTop();

			contentOffsetY = pos;

			$this.each(function(){
				var $element = $(this);
				var top = $element.offset().top;
				var left = $element.offset().left;

				var height = getHeight($element);
                var width = getWidth($element);

				// Check if totally above or totally below viewport
				if (top + height < pos || top > pos + windowHeight) {
					return;
				}
                
                //if (left + width < pos || left > pos + windowWidth) {
				//	return;
				//}

				var resp = movement(xpos, ypos, firstLeft, firstTop, pos, xspeedFactor, yspeedFactor, windowHeight, windowWidth);

				$this.css('background-position', resp);
				$this.css('-webkit-background-position', resp);
				$this.css({'opacity': changeOpacity(pos, speedOpacity)});
			});

		}

		var startTouchY, contentStartOffsetY, newY, interval;

		function updateIOS(e) {
			try {
				var currentY = e.originalEvent.touches[0].clientY;
				var deltaY = startTouchY- currentY;
				newY = deltaY + contentStartOffsetY;
				update(undefined, newY);
			} catch(err) {
				alert(err);
			}

		}	

		function updateIOS2() {
			$('.webcam').append(newY);
			update(undefined, newY);
		}

		function touchStart(e) {
			startTouchY = e.originalEvent.touches[0].clientY;
			contentStartOffsetY = contentOffsetY;
			//interval = setInterval(updateIOS2,100);

		}

		function touchEnd() {
			clearInterval(interval);

		}

		$window.bind('gesturestart', touchStart).resize(update);
		$window.bind('gestureend', touchEnd).resize(update);
		$window.bind('gesturemove', updateIOS).resize(update);
		$window.bind('scroll', update).resize(update);

		
		update();
	};

})(jQuery);
