var sound = false;
var waiting_sound = false;

function stop_sound() {
	if(sound)
	{
		toggle_sound();
		waiting_sound = true;
	}
}

function check_sound() {
	if(waiting_sound && !sound)
	{
		toggle_sound();
		waiting_sound = false;
	}
}

function toggle_sound() {
	Module.ccall('toggle_sound', 'number', [], []);
	sound = !sound;
}
	
document.getElementById('sound-control')
  .addEventListener('click', toggle_sound);