$(function(){

    var $window = $(window);

    // side bar
    $('.bs-sidebar').affix({
		offset: {
			top: function () { return $window.width() <= 980 ? 290 : 210 }, bottom: 270
		}
  	});

  	// Binding for RTD control panel
  	bindClick();
});

var bindClick = function () {
	if ($('.rst-current-version').length) {
		$('.rst-current-version').click(function() {
	  		$('.rst-other-versions').toggle()
	  	});
	} else {
		// Waiting to RTD for inserting the panel
		setTimeout(bindClick, 500);
	}
}