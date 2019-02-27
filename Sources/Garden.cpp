#include "cheats.hpp"
#include "CTRPluginFramework/Utils/Utils.hpp"

extern "C" void FixSaveFurn(void);
u32 g_FixSaveFurnAddr = 0;

static const char RTBP_ErrorMSG[] = "RTBP just tried to place/remove Building 0x%02X, which is invalid.\nPlease report this to the developers, along with:\n1)The building you selected on the list\n2)The building amount (Amount is: %d).";
static const char RTBP_EventMSG[] = "Only two event pwps can be placed at one time (to avoid issues with the game).\nPlease remove one of the existing event buildings to place a new one.";
static const char RTBP_0PWPMSG[] = "No buildings of this type can be found!";
static const char RTBP_0NUMMSG[] = "You have 0 buildings, how the hell are you removing one???";

namespace CTRPluginFramework
{
    void    SetNameTo(MenuEntry *entry)
    {
        Keyboard        keyboard("Name Changer\n\nEnter the name you'd like to have:");
        std::string     input;

        keyboard.OnInputChange([](Keyboard &keyboard, InputChangeEvent &event)
        {
            std::string &input = keyboard.GetInput();

            if (event.type == InputChangeEvent::CharacterAdded && Utils::GetSize(input) >= 6)
                Utils::RemoveLastChar(input);
        });
        if (keyboard.Open(input) != -1)
        {
            // Ask for a second line name if the name is smaller than 6 characters
            if (Utils::GetSize(input) < 6 
                && (MessageBox("Do you want your name to\nappear below the bubble ?", DialogType::DialogYesNo))())
                input.insert(0, 1, '\n');

            Player::GetInstance()->SetName(input);
        }
    }

    void    BuildingModifier(MenuEntry *entry)
    {
        static const StringVector pwptype = {"Normal PWPs", "Event PWPs"};
        static const StringVector MainOptions = { "Place a Building", "Remove a Building", "Move a Building" };
        StringVector options;
        u32 off_building = Game::Building;
        u32 counter = 0;
        u32 maxcounter = 0;

        if (*Game::Room != 0) {
            MessageBox("RTBM Error!", "You need to be in the Town for this to work.")();
            return;
        }
        
        Keyboard keyboard("Building Modifier:\n\nChoose an option.");
        keyboard.Populate(MainOptions);
        int userChoice = keyboard.Open();

        if (userChoice == -1)
            return;

        /* Place Bulding */
        else if (userChoice == 0) //Place
        {
            for (const Building &pwp : buildingIDS)
                options.push_back(pwp.Name);

            keyboard.GetMessage() = "Building Placer:\n\nWhich building would you like to place?";
            keyboard.Populate(options);
            int index = keyboard.Open();

            if (index == -1)
                return;

            Building CurPWP = buildingIDS[index];

            if ((CurPWP.id >= 0x12 && CurPWP.id <= 0x4B) || CurPWP.id > 0xFC) //Invalid Buildings
            {
                MessageBox("Building Placer: Error!", Format(RTBP_ErrorMSG, CurPWP.id, counter))();
                return;
            }

            else if (CurPWP.IsEvent) { //Event Building
                if (*(Game::BuildingSlots+1) == 2) {
                    MessageBox("Building Placer: Error!", RTBP_EventMSG)();
                    return;
                }

                counter = 56; //Event PWP are last two slots
                maxcounter = 58;
            }

            else { //Not Event Building
                counter = 0;
                maxcounter = 56;
            }

            //Increment counter until we find a empty slot
            while (READU8(off_building + (counter * 4)) != 0xFC && counter < maxcounter) {
                counter++;
            }

            if (counter == maxcounter)
                OSD::Notify(Color::Red << "Every Building Slot Is Filled!");

            else
            {
                Process::Write8(off_building + (counter * 4), CurPWP.id);
                Process::Write8(off_building + (counter * 4) + 2, static_cast<u8>(Game::WorldPos->x));
                Process::Write8(off_building + (counter * 4) + 3, static_cast<u8>(Game::WorldPos->y));
                OSD::Notify(Format("\"%s\" (0x%02X) Placed!", CurPWP.Name, CurPWP.id));
                OSD::Notify(Color::Green << "Reload the area to see effects.");
                if (CurPWP.IsEvent)
                    ADD8((Game::BuildingSlots+1), 1); //Increment Event Building Amount

                else
                    ADD8(Game::BuildingSlots, 1); ////Increment Normal Building Amount
            }
            return;
        }

        /* Remove Building */
        else if (userChoice == 1) //Remove
        {
            std::vector<u8> buildings;
            std::vector<bool> IsEvent;
            int start = 0, end = 0;

            keyboard.GetMessage() = "Building Remover:\n\nWhich building type would you like to remove?";
            keyboard.Populate(pwptype);
            int pwptypechoice = keyboard.Open();

            if (pwptypechoice == 0) { //Normal PWPs
                start = 0;
                end = 56;
            }

            else if (pwptypechoice == 1) { //Event PWPs
                start = 56;
                end = 58;
            }

            else return; //Keyboard abort or some weird error

            for (int i = start; i < end; i++)
            {
                u8 BuildingID = 0xFC; //Set default to empty
                Process::Read8(off_building + (i*4), BuildingID);
                if (BuildingID != 0xFC) { //check if a building is not empty
                    buildings.push_back(BuildingID);
                }
            }

            if (buildings.size() == 0) { //Possible case due to event pwps not always there
                MessageBox("Building Remover: Error!", RTBP_0PWPMSG)();
                return;
            }
            
            for (int i = 0; i < buildings.size(); i++)
            {
                for (const Building& building : buildingIDS)
                {
                    if (building.id != buildings[i])
                        continue;

                    options.push_back(building.Name);
                    IsEvent.push_back(building.IsEvent);
                    break;
                }
            }

            keyboard.GetMessage() = "Building Remover:\n\nWhich building would you like to remove?";
            keyboard.Populate(options);
            int index = keyboard.Open();

            if (index > -1) //user didn't abort
            {
                u8 id = buildings[index];
                const char* name = options[index].c_str();

                while ((READU8(off_building + (start*4) + (counter*4)) != id) && counter < buildings.size() && counter+start < end)
                    counter++;

                Process::Write8(off_building + (start*4) + (counter * 4), 0xFC);
                Process::Write8(off_building + (start*4) + (counter * 4) + 2, 0);
                Process::Write8(off_building + (start*4) + (counter * 4) + 3, 0);

                OSD::Notify(Format("\"%s\" (0x%02X) Removed!", name, id));
                OSD::Notify(Color::Green << "Reload the area to see effects.");

                if (IsEvent[index] && *(Game::BuildingSlots+1) > 0)
                    Process::Write8(reinterpret_cast<u32>(Game::BuildingSlots + 1), buildings.size()-1); //Decrement Event Building Amount

                else if (*Game::BuildingSlots > 0)
                    Process::Write8(reinterpret_cast<u32>(Game::BuildingSlots), buildings.size()-1); //Decrement Event Building Amount

                else MessageBox("Building Remover: Error!", RTBP_0NUMMSG)();
            }
            return;
        }

        /* Move Bulding */
        else if (userChoice == 2) //Move
        {
            std::vector<u8> buildings;
            int start = 0, end = 0;

            keyboard.GetMessage() = "Building Mover:\n\nWhich building type would you like to move?";
            keyboard.Populate(pwptype);
            int pwptypechoice = keyboard.Open();

            if (pwptypechoice == -1) //abort
                return;

            else if (pwptypechoice == 0) {
                start = 0;
                end = 56;
            }

            else if (pwptypechoice == 1) {
                start = 56;
                end = 58;
            }

            for (int i = start; i < end; i++)
            {
                u8 BuildingID = 0xFC; //Set default to empty
                Process::Read8(off_building + (i*4), BuildingID);
                if (BuildingID != 0xFC) { //check if a building is not empty
                    buildings.push_back(BuildingID);
                }
            }

            if (buildings.size() == 0) { //Possible case due to event pwps not always there
                MessageBox("Building Mover: Error!", RTBP_0PWPMSG)();
                return;
            }
            
            for (int i = 0; i < buildings.size(); i++)
            {
                for (const Building& building : buildingIDS)
                {
                    if (building.id != buildings[i])
                        continue;

                    options.push_back(building.Name);
                    break;
                }
            }

            keyboard.GetMessage() = "Building Mover:\n\nWhich building would you like to move?";
            keyboard.Populate(options);
            int index = keyboard.Open();

            if (index > -1) //user didn't abort
            {
                u8 id = buildings[index];
                const char* name = options[index].c_str();

                while (*(u8 *)(off_building + (start*4) + (counter * 4)) != id && counter < buildings.size() && counter+start < end)
                    counter++;

                Process::Write8(off_building + (start*4) + (counter * 4) + 2, static_cast<u8>(Game::WorldPos->x));
                Process::Write8(off_building + (start*4) + (counter * 4) + 3, static_cast<u8>(Game::WorldPos->y));

                OSD::Notify(Format("\"%s\" (0x%02X) Moved to Current Position!", name, id));
                OSD::Notify(Color::Green << "Reload the area to see effects.");
            }
            return;
        }
    }

    void    GardenDumper(MenuEntry *entry)
    {
        File            file;
        Directory       dir("dumps", true);
        Keyboard        keyboard("GardenRam Dumper\n\nName the dump you'd like to create.");
        std::string     input;

        if (keyboard.Open(input) != -1)
        {
            // Add extension to the name if user didn't
            if (input.find(".dat") == std::string::npos)
                input += ".dat";

            // Let's create and open the file
            if (dir.OpenFile(file, input, File::RWC) == 0)
            {
                // Dump to the file
                if (file.Dump(Game::Garden, 0x89B00) == 0) // Success
                    (MessageBox("File dumped to:\n" + file.GetFullName()))();
                else // Failed
                    MessageBox("Error\nDump failed.")();
            }

            file.Flush();
            file.Close();
        }
    }

    void    GardenRestore(MenuEntry *entry)
    {
        File            file;
        Directory       dir("dumps");
        StringVector    list;

        if (dir.ListFiles(list, ".dat") == Directory::OPResult::NOT_OPEN)
        {
            MessageBox("Error\nCouldn't open dumps folder.")();
            return;
        }

        if (list.empty())
        {
            MessageBox("Error\nCouldn't find any dumps!")();
            return;
        }

        Keyboard    keyboard("Select which dump to restore.", list);
        int         userChoice = keyboard.Open();

        if (userChoice != -1)
        {
            if (dir.OpenFile(file, list[userChoice], File::RWC) == 0)
            {
                if (file.Inject(Game::Garden, 0x89A80) == 0) {
                    MessageBox("Successfully restored your save !")();
                    g_FixSaveFurnAddr = Game::Internal_FurnFix;
                    u32 orig[1] = {0};
                    Process::Patch(g_FixSaveFurnAddr+0x41C, 0xE1A00000, orig); //Must be patched, otherwise game crashes
                    FixSaveFurn(); //Calls func to call Game's internal function to fix furniture
                    Process::Patch(g_FixSaveFurnAddr+0x41C, orig[0]); //Unpatch so game doesn't have a potential fit when it calls the func itself
                }
                else
                    MessageBox("Error\nError injecting dump!")();
            }
            else
                MessageBox("Error\nCouldn't open the dump!")();
        }
    }

    void    ChangeNativeFruit(MenuEntry *entry)
    {
        static const StringVector list =
        {
            "Apples",
            "Oranges",
            "Pears",
            "Peaches",
            "Cherries",
            "Coconuts",
            "Durians",
            "Lemons",
            "Lychee",
            "Mango",
            "Persimmon",
            "Bananas"
        };

        Keyboard    keyboard("What fruit would you like ?", list);
        int         userChoice = keyboard.Open();

        if (userChoice != -1)
            Process::Write8(Game::TownFruit, userChoice + 1);
    }

    void    PWPUnlock(MenuEntry *entry)
    {
        for (int i = 0; i < 3; i++)
        {
            Process::Write32(Game::PWP + (i * 4), 0xFFFFFFFF);
        }
        OSD::Notify("All PWPs Unlocked!", Color::Green, Color::Black);
        entry->Disable();
    }

    void    Permit(MenuEntry *entry)
    {
        Process::Write32(Game::Permit, 0xD7DFC900);
        OSD::Notify("Permit now 100%!", Color::Green, Color::Black);
        entry->Disable();
    }

    void    MaxTreeSize(MenuEntry *entry)
    {
        Process::Write8(Game::TownTree, 0x7);
        Process::Write32(Game::TownTree + 0x1632A, 0x10000000);
        Process::Write16(Game::TownTree + 0x163B8, 0x686);
    }

    void    ChangeGrass(MenuEntry *entry)
    {
        static const StringVector list =
        {
            "Triangle",
            "Circle",
            "Square"
        };

        Keyboard    keyboard("What grass type would you like ?", list);
        int         userChoice = keyboard.Open();

        if (userChoice != -1)
            Process::Write8(Game::TownGrass, userChoice);
    }
}