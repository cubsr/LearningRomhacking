#!/usr/bin/env python3
"""
Script to replace wild encounters with cuter Pokemon while maintaining level scaling and type coverage.
"""

import json
import random
import sys
from typing import Dict, List, Tuple

# Cute Pokemon organized by level ranges and type coverage
CUTE_POKEMON_BY_LEVEL = {
    "1-5": [
        "SPECIES_PIKACHU", "SPECIES_CLEFAIRY", "SPECIES_JIGGLYPUFF", "SPECIES_EEVEE",
        "SPECIES_VULPIX", "SPECIES_GROWLITHE", "SPECIES_MEOWTH", "SPECIES_PSYDUCK",
        "SPECIES_POLIWAG", "SPECIES_MAGIKARP", "SPECIES_STARYU", "SPECIES_HORSEA"
    ],
    "6-15": [
        "SPECIES_PIKACHU", "SPECIES_CLEFAIRY", "SPECIES_JIGGLYPUFF", "SPECIES_EEVEE",
        "SPECIES_VULPIX", "SPECIES_GROWLITHE", "SPECIES_MEOWTH", "SPECIES_PSYDUCK",
        "SPECIES_POLIWAG", "SPECIES_MAGIKARP", "SPECIES_STARYU", "SPECIES_HORSEA",
        "SPECIES_GOLDEEN", "SPECIES_CUBONE", "SPECIES_EXEGGCUTE", "SPECIES_VOLTORB",
        "SPECIES_MAGNEMITE"
    ],
    "16-30": [
        "SPECIES_PIKACHU", "SPECIES_CLEFAIRY", "SPECIES_JIGGLYPUFF", "SPECIES_EEVEE",
        "SPECIES_VULPIX", "SPECIES_GROWLITHE", "SPECIES_MEOWTH", "SPECIES_PSYDUCK",
        "SPECIES_POLIWAG", "SPECIES_MAGIKARP", "SPECIES_STARYU", "SPECIES_HORSEA",
        "SPECIES_GOLDEEN", "SPECIES_CUBONE", "SPECIES_EXEGGCUTE", "SPECIES_VOLTORB",
        "SPECIES_MAGNEMITE", "SPECIES_DITTO", "SPECIES_PORYGON", "SPECIES_CHANSEY",
        "SPECIES_LAPRAS", "SPECIES_SNORLAX"
    ],
    "31-50": [
        "SPECIES_PIKACHU", "SPECIES_CLEFAIRY", "SPECIES_JIGGLYPUFF", "SPECIES_EEVEE",
        "SPECIES_VULPIX", "SPECIES_GROWLITHE", "SPECIES_MEOWTH", "SPECIES_PSYDUCK",
        "SPECIES_POLIWAG", "SPECIES_MAGIKARP", "SPECIES_STARYU", "SPECIES_HORSEA",
        "SPECIES_GOLDEEN", "SPECIES_CUBONE", "SPECIES_EXEGGCUTE", "SPECIES_VOLTORB",
        "SPECIES_MAGNEMITE", "SPECIES_DITTO", "SPECIES_PORYGON", "SPECIES_CHANSEY",
        "SPECIES_LAPRAS", "SPECIES_SNORLAX"
    ],
    "51-100": [
        "SPECIES_PIKACHU", "SPECIES_CLEFAIRY", "SPECIES_JIGGLYPUFF", "SPECIES_EEVEE",
        "SPECIES_VULPIX", "SPECIES_GROWLITHE", "SPECIES_MEOWTH", "SPECIES_PSYDUCK",
        "SPECIES_POLIWAG", "SPECIES_MAGIKARP", "SPECIES_STARYU", "SPECIES_HORSEA",
        "SPECIES_GOLDEEN", "SPECIES_CUBONE", "SPECIES_EXEGGCUTE", "SPECIES_VOLTORB",
        "SPECIES_MAGNEMITE", "SPECIES_DITTO", "SPECIES_PORYGON", "SPECIES_CHANSEY",
        "SPECIES_LAPRAS", "SPECIES_SNORLAX"
    ]
}

# Track Pokemon usage to ensure variety
pokemon_usage_count = {}

def get_level_range(level: int) -> str:
    """Get the level range string for a given level."""
    if level <= 5:
        return "1-5"
    elif level <= 15:
        return "6-15"
    elif level <= 30:
        return "16-30"
    elif level <= 50:
        return "31-50"
    else:
        return "51-100"

def get_cute_pokemon_for_level(level: int) -> str:
    """Get a cute Pokemon appropriate for the given level."""
    level_range = get_level_range(level)
    available_pokemon = CUTE_POKEMON_BY_LEVEL[level_range]
    
    # Prefer Pokemon that haven't been used much
    unused_pokemon = [p for p in available_pokemon if pokemon_usage_count.get(p, 0) < 3]
    
    if unused_pokemon:
        chosen_pokemon = random.choice(unused_pokemon)
    else:
        # If all Pokemon have been used 3+ times, choose randomly
        chosen_pokemon = random.choice(available_pokemon)
    
    # Update usage count
    pokemon_usage_count[chosen_pokemon] = pokemon_usage_count.get(chosen_pokemon, 0) + 1
    
    return chosen_pokemon

def replace_encounter_mons(encounter_data: Dict) -> Dict:
    """Replace Pokemon in an encounter with cute alternatives."""
    if "mons" in encounter_data:
        for mon in encounter_data["mons"]:
            if "species" in mon:
                # Get the average level for this encounter
                min_level = mon.get("min_level", 1)
                max_level = mon.get("max_level", 1)
                avg_level = (min_level + max_level) // 2
                
                # Replace with cute Pokemon
                mon["species"] = get_cute_pokemon_for_level(avg_level)
    
    return encounter_data

def process_wild_encounters(input_file: str, output_file: str):
    """Process the wild encounters JSON file and replace with cute Pokemon."""
    print(f"Loading wild encounters from {input_file}...")
    
    with open(input_file, 'r') as f:
        data = json.load(f)
    
    print("Processing encounters...")
    
    # Process each encounter group
    for group in data.get("wild_encounter_groups", []):
        if "encounters" in group:
            for encounter in group["encounters"]:
                # Process land encounters
                if "land_mons" in encounter:
                    encounter["land_mons"] = replace_encounter_mons(encounter["land_mons"])
                
                # Process water encounters
                if "water_mons" in encounter:
                    encounter["water_mons"] = replace_encounter_mons(encounter["water_mons"])
                
                # Process rock smash encounters
                if "rock_smash_mons" in encounter:
                    encounter["rock_smash_mons"] = replace_encounter_mons(encounter["rock_smash_mons"])
                
                # Process fishing encounters
                if "fishing_mons" in encounter:
                    encounter["fishing_mons"] = replace_encounter_mons(encounter["fishing_mons"])
                
                # Process hidden encounters
                if "hidden_mons" in encounter:
                    encounter["hidden_mons"] = replace_encounter_mons(encounter["hidden_mons"])
    
    print(f"Saving updated encounters to {output_file}...")
    
    with open(output_file, 'w') as f:
        json.dump(data, f, indent=2)
    
    print("Done! Wild encounters have been replaced with cute Pokemon.")
    print(f"Pokemon usage summary:")
    for pokemon, count in sorted(pokemon_usage_count.items()):
        print(f"  {pokemon}: {count} encounters")

def main():
    if len(sys.argv) != 3:
        print("Usage: python replace_wild_encounters.py <input_file> <output_file>")
        print("Example: python replace_wild_encounters.py src/data/wild_encounters.json src/data/wild_encounters_cute.json")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    
    try:
        process_wild_encounters(input_file, output_file)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
