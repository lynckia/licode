
$(document).ready(function(){
	
	$('.menu li a').tooltip();
	
	$('.social-icons a').tooltip();
	
	$(".collapse").collapse();
	
	$('.nav-tabs a').click(function (e) {
	  e.preventDefault();
	  $(this).tab('show');
	})
	
		
	$('#phone_menu').change(function(){
		var triggerItem = $(this).attr('value');
		window.location=triggerItem;
	});
	
	$('.new_project .thumbnail_holder img').click(function(){
		
		var item_id = $(this).attr('data-id');
		
		$('.new_project .active_image img').css('display','none');
		$('.new_project .active_image img:nth-child('+item_id+')').fadeIn(800);
		
	});
	
	
});




$(document).ready(function() {

if ($.fn.cssOriginal!=undefined)
	$.fn.css = $.fn.cssOriginal;

	$('.fullwidthbanner').revolution(
		{
			delay:9000,
			startwidth:960,
			startheight:500,

			onHoverStop:"off",						// Stop Banner Timet at Hover on Slide on/off

			thumbWidth:100,							// Thumb With and Height and Amount (only if navigation Tyope set to thumb !)
			thumbHeight:50,
			thumbAmount:3,

			hideThumbs:0,
			navigationType:"bullet",					//bullet, thumb, none, both	 (No Shadow in Fullwidth Version !)
			navigationArrows:"verticalcentered",		//nexttobullets, verticalcentered, none
			navigationStyle:"round",				//round,square,navbar

			touchenabled:"on",						// Enable Swipe Function : on/off

			navOffsetHorizontal:0,
			navOffsetVertical:20,

			stopAtSlide:-1,							// Stop Timer if Slide "x" has been Reached. If stopAfterLoops set to 0, then it stops already in the first Loop at slide X which defined. -1 means do not stop at any slide. stopAfterLoops has no sinn in this case.
			stopAfterLoops:-1,						// Stop Timer if All slides has been played "x" times. IT will stop at THe slide which is defined via stopAtSlide:x, if set to -1 slide never stop automatic



			fullWidth:"on",

			shadow:0								//0 = no Shadow, 1,2,3 = 3 Different Art of Shadows -  (No Shadow in Fullwidth Version !)

		});

		
		
		


});

