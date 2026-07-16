InsideOfTruck_MapScripts::
	map_script MAP_SCRIPT_ON_LOAD, InsideOfTruck_OnLoad
	map_script MAP_SCRIPT_ON_RESUME, InsideOfTruck_OnResume
	.byte 0

InsideOfTruck_OnLoad:
	setmetatile 4, 1, METATILE_InsideOfTruck_ExitLight_Top, FALSE
	setmetatile 4, 2, METATILE_InsideOfTruck_ExitLight_Mid, FALSE
	setmetatile 4, 3, METATILE_InsideOfTruck_ExitLight_Bottom, FALSE
	end

InsideOfTruck_OnResume:
	setstepcallback STEP_CB_TRUCK
	end

InsideOfTruck_EventScript_SetIntroFlags::
	lockall
	call InsideOfTruck_EventScript_RandomizerPrompt
	setflag FLAG_HIDE_MAP_NAME_POPUP
	checkplayergender
	goto_if_eq VAR_RESULT, MALE, InsideOfTruck_EventScript_SetIntroFlagsMale
	goto_if_eq VAR_RESULT, FEMALE, InsideOfTruck_EventScript_SetIntroFlagsFemale
	end

InsideOfTruck_EventScript_SetIntroFlagsMale::
	setrespawn HEAL_LOCATION_LITTLEROOT_TOWN_BRENDANS_HOUSE_2F
	setvar VAR_LITTLEROOT_INTRO_STATE, 1
	setflag FLAG_HIDE_LITTLEROOT_TOWN_MAYS_HOUSE_MOM
	setflag FLAG_HIDE_LITTLEROOT_TOWN_MAYS_HOUSE_TRUCK
	setflag FLAG_HIDE_LITTLEROOT_TOWN_BRENDANS_HOUSE_RIVAL_MOM
	setflag FLAG_HIDE_LITTLEROOT_TOWN_BRENDANS_HOUSE_RIVAL_SIBLING
	setflag FLAG_HIDE_LITTLEROOT_TOWN_BRENDANS_HOUSE_2F_POKE_BALL
	setvar VAR_LITTLEROOT_HOUSES_STATE_BRENDAN, 1
	setdynamicwarp MAP_LITTLEROOT_TOWN, 3, 10
	releaseall
	end

InsideOfTruck_EventScript_SetIntroFlagsFemale::
	setrespawn HEAL_LOCATION_LITTLEROOT_TOWN_MAYS_HOUSE_2F
	setvar VAR_LITTLEROOT_INTRO_STATE, 2
	setflag FLAG_HIDE_LITTLEROOT_TOWN_BRENDANS_HOUSE_MOM
	setflag FLAG_HIDE_LITTLEROOT_TOWN_BRENDANS_HOUSE_TRUCK
	setflag FLAG_HIDE_LITTLEROOT_TOWN_MAYS_HOUSE_RIVAL_MOM
	setflag FLAG_HIDE_LITTLEROOT_TOWN_MAYS_HOUSE_RIVAL_SIBLING
	setflag FLAG_HIDE_LITTLEROOT_TOWN_MAYS_HOUSE_2F_POKE_BALL
	setvar VAR_LITTLEROOT_HOUSES_STATE_MAY, 1
	setdynamicwarp MAP_LITTLEROOT_TOWN, 12, 10
	releaseall
	end

@ One-time randomizer setup, asked during the opening truck ride.
@ Builds a RANDOMIZATION_FLAG_* mask in VAR_TEMP_0, then applies it.
InsideOfTruck_EventScript_RandomizerPrompt::
	setvar VAR_TEMP_0, 0
	msgbox InsideOfTruck_Text_RandomizeEncounters, MSGBOX_YESNO
	call_if_eq VAR_RESULT, YES, InsideOfTruck_EventScript_RandoAddEncounters
	msgbox InsideOfTruck_Text_RandomizeTrainers, MSGBOX_YESNO
	call_if_eq VAR_RESULT, YES, InsideOfTruck_EventScript_RandoAddTrainers
	msgbox InsideOfTruck_Text_RandomizeAbilities, MSGBOX_YESNO
	call_if_eq VAR_RESULT, YES, InsideOfTruck_EventScript_RandoAddAbilities
	msgbox InsideOfTruck_Text_RandomizeMoves, MSGBOX_YESNO
	call_if_eq VAR_RESULT, YES, InsideOfTruck_EventScript_RandoAddMoves
	msgbox InsideOfTruck_Text_RandomizeItems, MSGBOX_YESNO
	call_if_eq VAR_RESULT, YES, InsideOfTruck_EventScript_RandoAddItems
	copyvar VAR_0x8004, VAR_TEMP_0
	special ApplyRandomizationChoices
	setvar VAR_TEMP_0, 0
	return

InsideOfTruck_EventScript_RandoAddEncounters::
	addvar VAR_TEMP_0, 7    @ RANDOMIZATION_FLAG_WILD | STATIC | TRADES
	return

InsideOfTruck_EventScript_RandoAddTrainers::
	addvar VAR_TEMP_0, 8    @ RANDOMIZATION_FLAG_TRAINERS
	return

InsideOfTruck_EventScript_RandoAddAbilities::
	addvar VAR_TEMP_0, 16   @ RANDOMIZATION_FLAG_ABILITIES
	return

InsideOfTruck_EventScript_RandoAddMoves::
	addvar VAR_TEMP_0, 32   @ RANDOMIZATION_FLAG_MOVES
	return

InsideOfTruck_EventScript_RandoAddItems::
	addvar VAR_TEMP_0, 64   @ RANDOMIZATION_FLAG_ITEMS
	return

InsideOfTruck_Text_RandomizeEncounters:
	.string "RANDOMIZER SETUP\p"
	.string "Randomize wild POKéMON, gifts,\n"
	.string "and trades?$"

InsideOfTruck_Text_RandomizeTrainers:
	.string "Randomize TRAINERS' POKéMON?$"

InsideOfTruck_Text_RandomizeAbilities:
	.string "Randomize POKéMON abilities?$"

InsideOfTruck_Text_RandomizeMoves:
	.string "Randomize POKéMON movesets?\p"
	.string "Moves will favor each POKéMON's\n"
	.string "own type.$"

InsideOfTruck_Text_RandomizeItems:
	.string "Randomize items found in\n"
	.string "ITEM BALLS?$"

InsideOfTruck_EventScript_MovingBox::
	msgbox InsideOfTruck_Text_BoxPrintedWithMonLogo, MSGBOX_SIGN
	end

InsideOfTruck_Text_BoxPrintedWithMonLogo:
	.string "The box is printed with a POKéMON logo.\p"
	.string "It's a POKéMON brand moving and\n"
	.string "delivery service.$"

