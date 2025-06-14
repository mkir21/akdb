/**
@file privileges.c Provides functions for privileges
 */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */
#include "privileges.h"
#include <unistd.h>
#include "../auxi/constants.h"

/** 
 * @author Luka Balažinec
 * @brief Hashes a plain text password using SHA-256.
 * Generates a SHA-256 hash from the given password and stores the result
 * as a hexadecimal string in the provided buffer.
 * @param password Plain text password to hash.
 * @param hashed Buffer to store the resulting hash (at least 65 bytes).
 */
#include <openssl/sha.h>
#include <string.h>
#include <stdio.h>
void hash_password(const char *password, char *hashed) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char *)password, strlen(password), hash);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(hashed + (i * 2), "%02x", hash[i]);
    }
}


/**
 * @author Kristina Takač, edited by Borna Romić, update by Luka Balažinec, updated by Marko Kir
 * @brief  Inserts a new user in the AK_user table 
 * @param *username Username of the user to be added
 * @param *password Hashed password of the user to be added (SHA-256 hash as a string)
 * @param set_id Object ID of the new user
 * @return user_id
 */
int AK_user_add(char *username, char *password, int set_id) {
    AK_PRO;
    char *tblName = "AK_user";

    int usernameCheck = AK_user_get_id(username);
    if (usernameCheck != EXIT_ERROR) {
        printf("Username '%s' is not available!\n", username);
        AK_EPI;
        return "exists"; 
    }

    char hashed_password[SHA256_DIGEST_LENGTH * 2 + 1] = {0};
    hash_password(password, hashed_password);

    struct list_node *row_root = (struct list_node *) AK_malloc(sizeof(struct list_node));
    AK_Init_L3(&row_root);

    int user_id = AK_get_id();
    if (set_id != 0) user_id = set_id;

    AK_Insert_New_Element(TYPE_INT, &user_id, tblName, "obj_id", row_root);
    AK_Insert_New_Element(TYPE_VARCHAR, username, tblName, "username", row_root);
    AK_Insert_New_Element(TYPE_VARCHAR, hashed_password, tblName, "password", row_root);

    AK_insert_row(row_root);

    printf("\nAdded user '%s' under ID %d!\n\n", username, user_id);

    AK_free(row_root);
    AK_EPI;
    return user_id;
}

/**
 * @author Kristina Takač, updated by Barbara Tatai (fix leaks)
 * @brief  Function that returns an ID of the given user
 * @param *username username of user whose id we are looking for
 * @return user_id, otherwise EXIT_ERROR
 */
int AK_user_get_id(char *username) {
    AK_PRO;
    int i = 0, id = EXIT_ERROR;
    struct list_node *row;
    while ((row = (struct list_node *) AK_get_row(i, "AK_user")) != NULL && id == EXIT_ERROR) {
        struct list_node *elem_in_strcmp = AK_GetNth_L2(2, row);
        if (strcmp(elem_in_strcmp->data, username) == 0) {
            id = (int) *(AK_GetNth_L2(1, row)->data);            
        }
        i++;
        AK_DeleteAll_L3(&row);
        AK_free(row);  
    }
    AK_EPI;
    return id;
}

/**
 * @author Fran Mikolić, update by Luka Balažinec
 * @brief  Function that checks if a user exists with the given password
 * @param *username Username of the user whose password we are checking
 * @param *password Hashed password (SHA-256 hash as a string) of the given username to be checked
 * @return check 0 if false or 1 if true
 */
int AK_user_check_pass(char *username, char *password) {
    AK_PRO;
    int user_id = AK_user_get_id(username);

    if (user_id == EXIT_ERROR) {
        printf("User '%s' does not exist!\n", username);
        AK_EPI;
        return 0;
    }
    
    struct list_node *row = NULL;
    int i = 0;
    while ((row = (struct list_node *) AK_get_row(i, "AK_user")) != NULL) {
        int current_id = *(int *)AK_GetNth_L2(1, row)->data;
        if (current_id == user_id) {
            break;
        }
        AK_DeleteAll_L3(&row);
        AK_free(row);
        row = NULL;
        i++;
    }
    if (row == NULL) {
        printf("Error fetching user data for user_id: %d!\n", user_id);
        AK_EPI;
        return 0;
    }

    char hashed_input[SHA256_DIGEST_LENGTH * 2 + 1] = {0};
    hash_password(password, hashed_input);

    char *stored_password = (char *) AK_GetNth_L2(3, row)->data;
    stored_password[strcspn(stored_password, "\n")] = '\0';
    
    while (isspace((unsigned char)*stored_password)) stored_password++;
    char *end = stored_password + strlen(stored_password) - 1;
    while (end > stored_password && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';

    printf("Stored hash: '%s'\n", stored_password);
    printf("Input hash: '%s'\n", hashed_input);

    if (strcmp(stored_password, hashed_input) == 0) {
        printf("Login successful!\n");
        AK_EPI;
        return 1;
    } else {
        printf("Incorrect password!\n");
        AK_EPI;
        return 0;
    }
}

/**
 * @author Ljubo Barać
 * @brief Function that removes the given user
 * @param name Name of the user to be removed
 * @return EXIT_SUCCESS or EXIT_ERROR
 */
int AK_user_remove_by_name(char *name) {
    AK_PRO;
    int user_id = AK_user_get_id(name);

    struct list_node *row_root = (struct list_node *) AK_malloc(sizeof (struct list_node));
    AK_Init_L3(&row_root);
    AK_Update_Existing_Element(TYPE_VARCHAR, name, "AK_user", "username", row_root);
    int result = AK_delete_row(row_root);
    printf("\nRemoved user '%s' under ID %d!\n", name, user_id);

    AK_EPI;
    return result;
}

/**
 * @author Ljubo Barać, update by Lidija Lastavec, 
 * update by Marko Flajšek, update by Luka Balažinec
 * @brief Function that renames a given user
 * @param old_name Name of the user to be renamed
 * @param new_name New name of the user
 * @param password Password of the user to be renamed (hashed as a string)
 * @return EXIT_SUCCESS or EXIT_ERROR
 */
int AK_user_rename(char *old_name, char *new_name, char *password) {
    AK_PRO;
    int result = 0;
    int user_id = AK_user_get_id(old_name);

    result = AK_user_remove_by_name(old_name);
    if (result == EXIT_ERROR) {
        printf("Failed to remove old user '%s'.\n", old_name);
        AK_EPI;
        return result;
    }

    result = AK_user_add(new_name, password, user_id);
    if (result == EXIT_ERROR) {
        printf("Failed to add new user '%s'.\n", new_name);
        AK_EPI;
        return result;
    }

    printf("Renamed user '%s' to '%s' under ID %d!\n", old_name, new_name, user_id);

    AK_EPI;
    return result;
}

/**
 * @author Kristina Takač, edited by Ljubo Barać, Borna Romić
 * @brief Function that adds a new group
 * @param *name name of group to be added
 * @param set_id non default id to be passed
 * @return id of group                                                      
 */
int AK_group_add(char *name, int set_id) {
    AK_PRO;
    char *tblName = "AK_group";
    int groupNameCheck;
    char check = "";
   

    groupNameCheck = AK_group_get_id(name);
    if (groupNameCheck != EXIT_ERROR) {
        printf("Name '%s' is not available!\n", name);
        check = "taken";
        AK_EPI;
        return check;
    }

    struct list_node *row_root = (struct list_node *) AK_malloc(sizeof (struct list_node));
    AK_Init_L3(&row_root);
    int group_id = AK_get_id();
    if (set_id != 0) group_id = set_id;

    AK_Insert_New_Element(TYPE_INT, &group_id, tblName, "obj_id", row_root);
    AK_Insert_New_Element(TYPE_VARCHAR, name, tblName, "name", row_root);
    AK_insert_row(row_root);
    printf("Added group '%s' under ID %d!\n", name, group_id);

    AK_free(row_root);

    AK_EPI;
    return group_id;
}

/**
 * @author Kristina Takač.
 * @brief Function that returns the ID  from the given group name
 * @param *name name of group whose id we are looking for
 * @return id of group, otherwise EXIT_ERROR                                                      
 */
int AK_group_get_id(char *name) {
    AK_PRO;
    int i = 0;
    struct list_node *row;

    while ((row = (struct list_node *) AK_get_row(i, "AK_group")) != NULL) {
        struct list_node *elem_in_strcmp = AK_GetNth_L2(2, row);
        if (strcmp(elem_in_strcmp->data, name) == 0) {
            i = (int) * row->next->data;
            AK_free(row);
            AK_EPI;
            return i;
        }
        i++;
    }
    AK_free(row);

    AK_EPI;
    return EXIT_ERROR;
}

/**
 * @author Ljubo Barać
 * @brief Function that removes the given group
 * @param name Name of the group to be removed
 * @return EXIT_SUCCESS or EXIT_ERROR
 */
int AK_group_remove_by_name(char *name) {
    AK_PRO;
    struct list_node *row_root = (struct list_node *) AK_malloc(sizeof (struct list_node));
    int group_id = AK_group_get_id(name);

    AK_Init_L3(&row_root);
    AK_Update_Existing_Element(TYPE_VARCHAR, name, "AK_group", "name", row_root);
    int result = AK_delete_row(row_root);
    printf("Removed group '%s' under ID %d!\n", name, group_id);

    AK_EPI;
    return result;
}

/**
 * @author Ljubo Barać, update by Lidija Lastavec
 * @brief Function that renames the given group
 * @param old_name Name of the group to be renamed
 * @param new_name New name of the group
 * @return EXIT_SUCCESS or EXIT_ERROR
 */
int AK_group_rename(char *old_name, char *new_name) {
    AK_PRO;

    if (old_name == new_name) {
        printf("Please choose a different name!\n");
        AK_EPI;
        return EXIT_ERROR;
    }

    int result = 0;
    int *group_id = AK_group_get_id(old_name);

    result = AK_group_remove_by_name(old_name);
    result = AK_group_add(new_name, group_id);
    printf("Renamed group '%s' to '%s' under ID %d!\n", old_name, new_name, group_id);

    AK_EPI;
    return result;
}

/**
 * @author Kristina Takač, updated by Mario Peroković, inserting user id instead of username in AK_user_right, updated by Marko Flajšek,
 * updated by Marko Kir - inserting user id instead of name in AK_user_right
 * @brief Function that grants a specific privilege to the desired user on a given table
 * @param *username username of user to whom we want to grant privilege
 * @param *table name of table on which privilege will be granted to user
 * @param *right type of privilege which will be granted to user on given table
 * @return privilege_id or EXIT_ERROR if table or user aren't correct
 */
int AK_grant_privilege_user(char *username, char *table, char *right) {
    AK_PRO;
    int privilege_id;
    int table_id = AK_get_table_obj_id(table);
    int user_id = AK_user_get_id(username);

    if (table_id == EXIT_ERROR || user_id == EXIT_ERROR) {
        printf("Invalid table name or username!\n");
        AK_EPI;
        return EXIT_ERROR;
    }

    if (strcmp(right, "ALL") == 0) {
        char rights[100];
        strcpy(&rights[0], "UPDATE\0");
        strcpy(&rights[10], "DELETE\0");
        strcpy(&rights[20], "INSERT\0");
        strcpy(&rights[30], "SELECT\0");
        int i;

        for (i = 0; i < 4; i++) {
            struct list_node *row_root = (struct list_node *) AK_malloc(sizeof (struct list_node));
            AK_Init_L3(&row_root);
            privilege_id = AK_get_id();

            AK_Insert_New_Element(TYPE_INT, &privilege_id, "AK_user_right", "obj_id", row_root);
            AK_Insert_New_Element(TYPE_INT, &user_id, "AK_user_right", "user_id", row_root);
            AK_Insert_New_Element(TYPE_INT, &table_id, "AK_user_right", "artifact_id", row_root);
            AK_Insert_New_Element(TYPE_VARCHAR, &rights[i * 10], "AK_user_right", "right_type", row_root);
            AK_insert_row(row_root);
            printf("\nGranted privilege to %s data for user '%s' under ID %d on table '%s'!\n\n", right, username, user_id, table);

            AK_free(row_root);
        }
    } else {
        struct list_node *row_root = (struct list_node *) AK_malloc(sizeof (struct list_node));
        AK_Init_L3(&row_root);

        privilege_id = AK_get_id();
        AK_Insert_New_Element(TYPE_INT, &privilege_id, "AK_user_right", "obj_id", row_root);
        AK_Insert_New_Element(TYPE_INT, &user_id, "AK_user_right", "user_id", row_root);
        AK_Insert_New_Element(TYPE_INT, &table_id, "AK_user_right", "artifact_id", row_root);
        AK_Insert_New_Element(TYPE_VARCHAR, right, "AK_user_right", "right_type", row_root);
        AK_insert_row(row_root);
        printf("\nGranted privilege to %s data for user '%s' under ID %d on table '%s'!\n\n", right, username, user_id, table);

        AK_free(row_root);
    }

    AK_EPI;
    return privilege_id;
}

/**
 * @author Kristina Takač, updated by Mario Peroković - added comparing by table id, and use of user_id in AK_user_right,
 * updated by Marko Kir - fixed the issue with removing rows from table
 * @brief Function that revokes users privilege on the given table
 * @param *username username of user to whom we want to grant privilege
 * @param *table name of table on which privilege will be revoked from user
 * @param *right type of privilege which will be revoked from user on given table
 * @return EXIT_SUCCESS if privilege is revoked, EXIT_ERROR if it isn't
 */
int AK_revoke_privilege_user(char *username, char *table, char *right) {
    AK_PRO;
    int table_id = AK_get_table_obj_id(table);
    int user_id = AK_user_get_id(username);
    int result = EXIT_ERROR;

    if (table_id == EXIT_ERROR || user_id == EXIT_ERROR) {
        printf("Invalid table name or username!\n");
        AK_EPI;
        return EXIT_ERROR;
    }

    struct list_node *row_root = (struct list_node *) AK_malloc(sizeof(struct list_node));
    AK_Init_L3(&row_root);

    if (strcmp(right, "ALL") == 0) {
        // Define the array of individual rights
        char rights[4][10] = {"UPDATE", "DELETE", "INSERT", "SELECT"};
        for (int i = 0; i < 4; i++) {
            int i_row = 0;
            struct list_node *row;

            while ((row = (struct list_node *) AK_get_row(i_row, "AK_user_right")) != NULL) {
                struct list_node *user_elem = AK_GetNth_L2(2, row);
                struct list_node *table_elem = AK_GetNth_L2(3, row);
                struct list_node *right_elem = AK_GetNth_L2(4, row);

                if (*(int *)user_elem->data == user_id &&
                    *(int *)table_elem->data == table_id &&
                    strcmp((char *)right_elem->data, rights[i]) == 0) {
                    int obj_id = *(int *)AK_GetNth_L2(1, row)->data;
                    AK_Update_Existing_Element(TYPE_INT, &obj_id, "AK_user_right", "obj_id", row_root);
                    result = AK_delete_row(row_root);
                    if (result == EXIT_SUCCESS) {
                        printf("Revoked privilege '%s' for user '%s' under ID %d on table '%s' under ID %d!\n",
                               rights[i], username, user_id, table, table_id);
                    } else {
                        printf("Failed to revoke privilege '%s' for user '%s' under ID %d on table '%s' under ID %d!\n",
                               rights[i], username, user_id, table, table_id);
                    }
                }
                i_row++;
                AK_DeleteAll_L3(&row_root);
                AK_free(row);
            }
        }
        printf("Revoked all privileges for user '%s' under ID %d on table '%s'!\n", username, user_id, table);
    } else {
        int i_row = 0;
        struct list_node *row;

        while ((row = (struct list_node *) AK_get_row(i_row, "AK_user_right")) != NULL) {
            struct list_node *user_elem = AK_GetNth_L2(2, row);
            struct list_node *table_elem = AK_GetNth_L2(3, row);
            struct list_node *right_elem = AK_GetNth_L2(4, row);

            if (*(int *)user_elem->data == user_id &&
                *(int *)table_elem->data == table_id &&
                strcmp((char *)right_elem->data, right) == 0) {
                int obj_id = *(int *)AK_GetNth_L2(1, row)->data;
                AK_Update_Existing_Element(TYPE_INT, &obj_id, "AK_user_right", "obj_id", row_root);
                result = AK_delete_row(row_root);
                if (result == EXIT_SUCCESS) {
                    printf("Revoked privilege '%s' for user '%s' under ID %d on table '%s' under ID %d!\n",
                           right, username, user_id, table, table_id);
                } else {
                    printf("Failed to revoke privilege '%s' for user '%s' under ID %d on table '%s' under ID %d!\n",
                           right, username, user_id, table, table_id);
                }
                break;
            }
            i_row++;
            AK_DeleteAll_L3(&row_root);
            AK_free(row);
        }
    }

    if (result == EXIT_ERROR) {
        printf("User '%s' does not have privilege '%s' on table '%s'!\n", username, right, table);
    }

    AK_free(row_root);
    AK_EPI;
    return result;
}

/**
 * @author Jurica Hlevnjak, updated by Marko Flajšek, updated by Marko Kir - fixed the issue with removing rows from table
 * @brief Function that revokes ALL user's privileges on ALL tables (for DROP user)
 * @param username name of user from whom we want to revoke all privileges
 * @return EXIT_SUCCESS if privilege is revoked, EXIT_ERROR if it isn't
 */
int AK_revoke_all_privileges_user(char *username) {
    AK_PRO;
    int user_id = AK_user_get_id(username);
    int result;

    if (user_id == EXIT_ERROR) {
        printf("Invalid username!\n");
        AK_EPI;
        return EXIT_ERROR;
    }

    struct list_node *row_root = (struct list_node *) AK_malloc(sizeof (struct list_node));
    AK_Init_L3(&row_root);
    int i = 0;

    struct list_node *row;

    while ((row = (struct list_node *) AK_get_row(i, "AK_user_right")) != NULL) {
        struct list_node *user = AK_GetNth_L2(2, row);
        if ((int) *user->data == user_id) {
            AK_Update_Existing_Element(TYPE_INT, &user_id, "AK_user_right", "user_id", row_root);
            printf("Revoked all privileges for user '%s' under ID %d!\n", username, user_id);
            result = AK_delete_row(row_root);
        }
        i++;

        AK_DeleteAll_L3(&row_root);
        AK_free(row);
    }

    if (result == EXIT_ERROR) {
        AK_EPI;
        printf("User '%s' under ID %d doesn't have any privileges!\n", username, user_id);
        return EXIT_ERROR;
    }

    AK_EPI;
    return EXIT_SUCCESS;
}

/**
 * @author Kristina Takač.
 * @brief Function that grants a privilege to a given group on a given table
 * @param *groupname  name of group to which we want to grant privilege
 * @param *table name of table on which privilege will be granted to user
 * @param *right type of privilege which will be granted to user on given table
 * @return privilege_id or EXIT_ERROR if table or user aren't correct
 */
int AK_grant_privilege_group(char *groupname, char *table, char *right) {
    AK_PRO;
    int privilege_id;
    int table_id = AK_get_table_obj_id(table);
    int group_id = AK_group_get_id(groupname);

    if (table_id == EXIT_ERROR || group_id == EXIT_ERROR) {
        printf("Invalid table name or group name!\n");
        AK_EPI;
        return EXIT_ERROR;
    }

    if (strcmp(right, "ALL") == 0) {
        char rights[100];
        strcpy(&rights[0], "UPDATE\0");
        strcpy(&rights[10], "DELETE\0");
        strcpy(&rights[20], "INSERT\0");
        strcpy(&rights[30], "SELECT\0");
        int i;
        for (i = 0; i < 4; i++) {
            struct list_node *row_root = (struct list_node *) AK_malloc(sizeof (struct list_node));
            AK_Init_L3(&row_root);
            privilege_id = AK_get_id();

            AK_Insert_New_Element(TYPE_INT, &privilege_id, "AK_group_right", "obj_id", row_root);
            AK_Insert_New_Element(TYPE_INT, &group_id, "AK_group_right", "group_id", row_root);
            AK_Insert_New_Element(TYPE_INT, &table_id, "AK_group_right", "artifact_id", row_root);
            AK_Insert_New_Element(TYPE_VARCHAR, &rights[i * 10], "AK_group_right", "right_type", row_root);
            AK_insert_row(row_root);

            AK_free(row_root);
        }
        printf("\nGranted all privileges for group '%s' under ID %d on table '%s' under ID %d!\n\n", groupname, group_id, table, table_id);
    } else {
        struct list_node *row_root = (struct list_node *) AK_malloc(sizeof (struct list_node));
        AK_Init_L3(&row_root);

        privilege_id = AK_get_id();
        AK_Insert_New_Element(TYPE_INT, &privilege_id, "AK_group_right", "obj_id", row_root);
        AK_Insert_New_Element(TYPE_INT, &group_id, "AK_group_right", "group_id", row_root);
        AK_Insert_New_Element(TYPE_INT, &table_id, "AK_group_right", "artifact_id", row_root);
        AK_Insert_New_Element(TYPE_VARCHAR, right, "AK_group_right", "right_type", row_root);
        AK_insert_row(row_root);
        printf("\nGranted privilege to %s data for group '%s' under ID %d on table '%s' under ID %d!\n\n", right, groupname, group_id, table, table_id);

        AK_free(row_root);
    }

    AK_EPI;
    return EXIT_SUCCESS;
}

/**
 * @author Kristina Takač, updated by Mario Peroković - added comparing by table id, updated by Marko Kir - fixed the issue 
 * with removing rows from table
 * @brief Function that revokes a groups privilege on the given table 
 * @param *grounamep name of group which user belongs to
 * @param *table name of table on which privilege will be granted to group
 * @param *right type of privilege which will be granted to group on a given table
 * @return EXIT_SUCCESS if privilege is revoked, EXIT_ERROR if it isn't
 */
int AK_revoke_privilege_group(char *group_name, char *table, char *right) {
    AK_PRO;
    int table_id = AK_get_table_obj_id(table);
    int group_id = AK_group_get_id(group_name);
    int result = EXIT_ERROR;

    if (group_id == EXIT_ERROR || table_id == EXIT_ERROR) {
        printf("Invalid table name or group name!\n");
        AK_EPI;
        return EXIT_ERROR;
    }

    struct list_node *row_root = (struct list_node *) AK_malloc(sizeof(struct list_node));
    AK_Init_L3(&row_root);

    if (strcmp(right, "ALL") == 0) {
        // Define the array of individual rights
        char rights[4][10] = {"UPDATE", "DELETE", "INSERT", "SELECT"};
        for (int i = 0; i < 4; i++) {
            int i_row = 0;
            struct list_node *row;

            while ((row = (struct list_node *) AK_get_row(i_row, "AK_group_right")) != NULL) {
                struct list_node *group_elem = AK_GetNth_L2(2, row);
                struct list_node *table_elem = AK_GetNth_L2(3, row);
                struct list_node *right_elem = AK_GetNth_L2(4, row);

                if (*(int *)group_elem->data == group_id &&
                    *(int *)table_elem->data == table_id &&
                    strcmp((char *)right_elem->data, rights[i]) == 0) {
                    int obj_id = *(int *)AK_GetNth_L2(1, row)->data;
                    AK_Update_Existing_Element(TYPE_INT, &obj_id, "AK_group_right", "obj_id", row_root);
                    result = AK_delete_row(row_root);
                    if (result == EXIT_SUCCESS) {
                        printf("Revoked privilege '%s' for group '%s' under ID %d on table '%s' under ID %d!\n",
                               rights[i], group_name, group_id, table, table_id);
                    } else {
                        printf("Failed to revoke privilege '%s' for group '%s' under ID %d on table '%s' under ID %d!\n",
                               rights[i], group_name, group_id, table, table_id);
                    }
                }
                i_row++;
                AK_DeleteAll_L3(&row_root);
                AK_free(row);
            }
        printf("Revoked all privileges for group '%s' under ID %d on table '%s'!\n", group_name, group_id, table);
        }
    } else {
        int i_row = 0;
        struct list_node *row;

        while ((row = (struct list_node *) AK_get_row(i_row, "AK_group_right")) != NULL) {
            struct list_node *group_elem = AK_GetNth_L2(2, row);
            struct list_node *table_elem = AK_GetNth_L2(3, row);
            struct list_node *right_elem = AK_GetNth_L2(4, row);

            if (*(int *)group_elem->data == group_id &&
                *(int *)table_elem->data == table_id &&
                strcmp((char *)right_elem->data, right) == 0) {
                int obj_id = *(int *)AK_GetNth_L2(1, row)->data;
                AK_Update_Existing_Element(TYPE_INT, &obj_id, "AK_group_right", "obj_id", row_root);
                result = AK_delete_row(row_root);
                if (result == EXIT_SUCCESS) {
                    printf("Revoked privilege '%s' for group '%s' under ID %d on table '%s' under ID %d!\n",
                           right, group_name, group_id, table, table_id);
                } else {
                    printf("Failed to revoke privilege '%s' for group '%s' under ID %d on table '%s' under ID %d!\n",
                           right, group_name, group_id, table, table_id);
                }
                break;
            }
            i_row++;
            AK_DeleteAll_L3(&row_root);
            AK_free(row);
        }
    }

    if (result == EXIT_ERROR) {
        printf("Group '%s' does not have privilege '%s' on table '%s'!\n", group_name, right, table);
    }

    AK_free(row_root);
    AK_EPI;
    return result;
}

/**
 * @author Jurica Hlevnjak
 * @brief Function that revokes ALL privileges from the desired group on ALL tables (needed for DROP group)
 * @param groupname name of group from which we want to revoke all privileges
 * @return EXIT_SUCCESS if privilege is revoked, EXIT_ERROR if it isn't
 */
int AK_revoke_all_privileges_group(char *groupname) {
    AK_PRO;
    int group_id = AK_group_get_id(groupname);
    int result;

    if (group_id == EXIT_ERROR) {
        printf("Invalid group name!\n");
        AK_EPI;
        return EXIT_ERROR;
    }

    struct list_node *row_root = (struct list_node *) AK_malloc(sizeof (struct list_node));
    AK_Init_L3(&row_root);
    int i = 0;

    struct list_node *row;

    while ((row = (struct list_node *) AK_get_row(i, "AK_group_right")) != NULL) {
        struct list_node *group = AK_GetNth_L2(2, row);
        if ((int) *group->data == group_id) {
            AK_Update_Existing_Element(TYPE_INT, &group_id, "AK_group_right", "group_id", row_root);
            printf("Revoked all privilege for group '%s' under ID %d!\n\n", groupname, group_id);
            result = AK_delete_row(row_root);
        }
        i++;

        AK_DeleteAll_L3(&row_root);
        AK_free(row);
    }

    if (result == EXIT_ERROR) {
        AK_EPI;
        printf("Group '%s' under ID %d doesn't have any privileges!\n", groupname, group_id);
        return EXIT_ERROR;
    }

    AK_EPI;
    return EXIT_SUCCESS;
}

/**
 * @author Kristina Takač, updated by Mario Peroković, added verifying the existence of user in the group, updated by Maja Vračan 
 * @brief Function that puts the desired user in the given group
 * @param *user username of user which will be put in group
 * @param *group name of group in which user will be put
 * @return EXIT_SUCCESS or EXIT_ERROR if the user is already in the group
 */
int AK_add_user_to_group(char *user, char *group) {
    AK_PRO;
    int user_id = AK_user_get_id(user);
    int group_id = AK_group_get_id(group);
    int i = 0;
    struct list_node *row;

    if (group_id == EXIT_ERROR || user_id == EXIT_ERROR) {
        printf("Invalid group name or username!\n");
        AK_EPI;
        return EXIT_ERROR;
    }

    while ((row = (struct list_node *) AK_get_row(i, "AK_user_group")) != NULL) {

        // if user is already in group, return error
        if (user_id == (int) *row->next->data) {
            printf("User '%s' under ID %d is already a member of group '%s' under ID %d!\n", user, user_id, group, group_id);
            AK_EPI;
            return EXIT_ERROR;
        }
        i++;
    }

    struct list_node *row_root = (struct list_node *) AK_malloc(sizeof (struct list_node));
    AK_Init_L3(&row_root);

    AK_Insert_New_Element(TYPE_INT, &user_id, "AK_user_group", "user_id", row_root);
    AK_Insert_New_Element(TYPE_INT, &group_id, "AK_user_group", "group_id", row_root);
    AK_insert_row(row_root);
    printf("Added user '%s' under ID %d to group '%s' under ID %d!\n", user, user_id, group, group_id);

    AK_free(row_root);

    AK_EPI;
    return EXIT_SUCCESS;
}

/**
 * @author Jurica Hlevnjak, update by Lidija Lastavec
 * @brief Function that removes user from all groups. Used for DROP user.
 * @param user name of user
 * @return EXIT_SUCCESS or EXIT_ERROR
 */
int AK_remove_user_from_all_groups(char *user) {
    AK_PRO;

    struct list_node *row_root = (struct list_node *) AK_malloc(sizeof (struct list_node));
    AK_Init_L3(&row_root);

    int user_id = AK_user_get_id(user);
    int i = 0;
    int result;

    if (user_id == EXIT_ERROR) {
        printf("Invalid username!\n");
        AK_EPI;
        return EXIT_ERROR;
    }

    struct list_node *row;

    while ((row = (struct list_node *) AK_get_row(i, "AK_user_group")) != NULL) {
        struct list_node *user = AK_GetNth_L2(1, row);
        if (user_id == (int) *user->data) {
            AK_Update_Existing_Element(TYPE_INT, &user_id, "AK_user_group", "user_id", row_root);
            result = AK_delete_row(row_root);
            if (result == EXIT_ERROR) {
                printf("User '%s' under ID %d isn't a member of any group!\n", user, user_id);
                AK_EPI;
                return EXIT_ERROR;
            }
        }
        i++;

        AK_DeleteAll_L3(&row_root);
        AK_free(row);
    }

    printf("User '%s' under ID %d is removed from all groups!\n", user, user_id);
    AK_EPI;
    return EXIT_SUCCESS;
}

/**
 * @author Jurica Hlevnjak, update by Lidija Lastavec
 * @brief Function that removes all users from a group. Used for DROP group.
 * @param group name of group
 * @return EXIT_SUCCESS or EXIT_ERROR
 */
int AK_remove_all_users_from_group(char *group) {
    AK_PRO;

    struct list_node *row_root = (struct list_node *) AK_malloc(sizeof (struct list_node));
    AK_Init_L3(&row_root);

    int group_id = AK_group_get_id(group);
    int i = 0;
    int result;

    if (group_id == EXIT_ERROR) {
        printf("Invalid group name!\n");
        AK_EPI;
        return EXIT_ERROR;
    }

    struct list_node *row;

    while ((row = (struct list_node *) AK_get_row(i, "AK_user_group")) != NULL) {
        struct list_node *group = AK_GetNth_L2(2, row);
        if (group_id == (int) *group->data) {
            AK_Update_Existing_Element(TYPE_INT, &group_id, "AK_user_group", "group_id", row_root);
            result = AK_delete_row(row_root);
        }
        i++;
        if (result == EXIT_ERROR) {
            printf("Group '%s' under ID %d doesn't contain any users!", group, group_id);
            AK_EPI;
            return EXIT_ERROR;
        }

        AK_DeleteAll_L3(&row_root);
        AK_free(row);
    }

    AK_free(row_root);
    printf("Users deleted from group '%s' under ID %d!\n", group, group_id);
    AK_EPI;
    return EXIT_SUCCESS;
}

/**
 * @author Kristina Takač, updated by Marko Flajšek
 * @brief Function that checks whether the given user has a right for the given operation on the given table
 * @param *user username for which we want check privileges
 * @param *table name of table for which we want to check whether user has right on
 * @param *privilege privilege for which we want to check whether user has right for
 * @return EXIT_SUCCESS if user has right, EXIT_ERROR if user has no right
 */
int AK_check_privilege(char *username, char *table, char *privilege) {
    AK_PRO;
    int user_id = AK_user_get_id(username);
    int table_id = AK_get_table_obj_id(table);
    int i = 0;
    int number_of_groups = 0;
    int has_right = 0;
    int groups[100];

    if (table_id == EXIT_ERROR || user_id == EXIT_ERROR) {
        printf("Invalid table name or username!\n");
        AK_EPI;
        return EXIT_ERROR;
    }

    struct list_node *row;

    if (strcmp(privilege, "ALL") == 0) {

        int checking_privileges[4] = {0, 0, 0, 0};
        char found_privilege[10];
        while ((row = (struct list_node *) AK_get_row(i, "AK_user_right")) != NULL) {

            struct list_node *username_elem = AK_GetNth_L2(2, row);
            struct list_node *table_elem = AK_GetNth_L2(3, row);
            if (((int) * username_elem->data == user_id) && (table_id == (int) * table_elem->data)) {

                struct list_node *privilege_elem = AK_GetNth_L2(4, row);
                strcpy(found_privilege, privilege_elem->data);
                if (strcmp(found_privilege, "UPDATE") == 0)
                    checking_privileges[0] = 1;
                if (strcmp(found_privilege, "DELETE") == 0)
                    checking_privileges[1] = 1;
                if (strcmp(found_privilege, "INSERT") == 0)
                    checking_privileges[2] = 1;
                if (strcmp(found_privilege, "SELECT") == 0)
                    checking_privileges[3] = 1;
            }
            i++;
            AK_free(row);
        }
        for (i = 0; i < 4; i++) {
            if (checking_privileges[i] == 1) {
                has_right = 1;
            } else {
                has_right = 0;
                break;
            }
        }
        if (has_right == 1) {
            printf("User '%s' under ID %d has all privileges in the '%s' table under ID %d!", username, user_id, table, table_id);
            AK_EPI;
            return EXIT_SUCCESS;
        }

        i = 0;
        while ((row = (struct list_node *) AK_get_row(i, "AK_user_group")) != NULL) {
            struct list_node *user = AK_GetNth_L2(1, row);
            if (user_id == (int) * user->data) {
                struct list_node *group = AK_GetNth_L2(2, row);
                groups[number_of_groups] = (int) * group->data;
                number_of_groups++;
            }
            i++;
            AK_free(row);
        }
        // set "flags" to 0
        checking_privileges[0] = 0;
        checking_privileges[1] = 0;
        checking_privileges[2] = 0;
        checking_privileges[3] = 0;
        for (i = 0; i < number_of_groups; i++) {
            int j = 0;
            while ((row = (AK_list *) AK_get_row(j, "AK_group_right")) != NULL) {
                struct list_node *group_elem = AK_GetNth_L2(2, row);
                struct list_node *table_elem = AK_GetNth_L2(3, row);
                struct list_node *privilege_elem = AK_GetNth_L2(4, row);

                if ((groups[i] == (int) * group_elem->data) && (table_id == (int) * table_elem->data)) {
                    strcpy(found_privilege, privilege_elem->data);
                    if (strcmp(found_privilege, "UPDATE") == 0)
                        checking_privileges[0] = 1;
                    if (strcmp(found_privilege, "DELETE") == 0)
                        checking_privileges[1] = 1;
                    if (strcmp(found_privilege, "INSERT") == 0)
                        checking_privileges[2] = 1;
                    if (strcmp(found_privilege, "SELECT") == 0)
                        checking_privileges[3] = 1;
                }
                j++;
                AK_free(row);
            }
        }
        for (i = 0; i < 4; i++) {
            if (checking_privileges[i] == 1) {
                has_right = 1;
            } else {
                has_right = 0;
                break;
            }
        }
        if (has_right == 1) {
            printf("User '%s' under ID %d has all privileges in the '%s' table under ID %d!", username, user_id, table, table_id);
            AK_EPI;
            return EXIT_SUCCESS;
        }
        
    } else {        
        while ((row = (struct list_node *) AK_get_row(i, "AK_user_right")) != NULL) {           
            struct list_node *username_elem = AK_GetNth_L2(2, row);
            struct list_node *table_elem = AK_GetNth_L2(3, row);
            struct list_node *privilege_elem = AK_GetNth_L2(4, row);                

            if (((int) * username_elem->data == user_id) && (table_id == (int) * table_elem->data) && (strcmp(privilege_elem->data, privilege) == 0)) {

                has_right = 1;
                printf("User '%s' under ID %d has the right to %s data in the '%s' table under ID %d!", username, user_id, privilege, table, table_id);                
                AK_EPI;
                return EXIT_SUCCESS;
            }
            i++;
            AK_free(row);
        }
        i = 0;
        while ((row = (struct list_node *) AK_get_row(i, "AK_user_group")) != NULL) {
            struct list_node *user = AK_GetNth_L2(1, row);

            if (user_id == (int) * user->data) {
                struct list_node *group = AK_GetNth_L2(2, row);
                groups[number_of_groups] = (int) * group->data;
                number_of_groups++;
            }
            i++;
            AK_free(row);
        }
        for (i = 0; i < number_of_groups; i++) {
            int j = 0;

            while ((row = (struct list_node *) AK_get_row(j, "AK_group_right")) != NULL) {
                struct list_node *groups_elem = AK_GetNth_L2(2, row);
                struct list_node *table_elem = AK_GetNth_L2(3, row);
                struct list_node *privilege_elem = AK_GetNth_L2(4, row);
                if ((groups[i] == (int) * groups_elem->data) && (table_id == (int) * table_elem->data) && (strcmp(privilege_elem->data, privilege) == 0)) {
                    has_right = 1;
                    printf("User '%s' under ID %d has the right to %s data in the '%s' table under ID %d!", username, user_id, privilege, table, table_id);
                    AK_EPI;
                    return EXIT_SUCCESS;
                }
                j++;
                AK_free(row);
            }
        }
    }

    printf("User '%s' under ID %d has no right to %s data in the '%s' table under ID %d!", username, user_id, privilege, table, table_id);
    AK_EPI;
    return EXIT_ERROR;
}

/**
 * @author Jurica Hlevnjak, updated by Lidija Lastavec
 * @brief Function that checks if the user has any privileges or belongs to any group. Used in drop user for restriction.
 * @param user name of user
 * @return EXIT_ERROR or EXIT_SUCCESS
 */
int AK_check_user_privilege(char *user) {
    AK_PRO;
    int user_id = AK_user_get_id(user);
    int i = 0;

    if (user_id == EXIT_ERROR) {
        printf("Invalid username!\n");
        AK_EPI;
        return EXIT_ERROR;
    }

    struct list_node *row;
    int privilege = 0;

    while ((row = (struct list_node *) AK_get_row(i, "AK_user_right")) != NULL) {
        struct list_node *user_elem = AK_GetNth_L2(2, row);
        if ((int) *user_elem->data == user_id) {
            privilege = PRIVILEGE_GRANTED;
            printf("User '%s' under ID %d has some privileges!", user, user_id);
            AK_EPI;
            return EXIT_SUCCESS;
        }
        i++;
        AK_free(row);
    }

    int j = 0;
    while ((row = (struct list_node *) AK_get_row(j, "AK_user_group")) != NULL) {
        struct list_node *user_elem = AK_GetNth_L2(1, row);
        if ((int) *user_elem->data == user_id) {
            privilege = 1;
            printf("User '%s' under ID %d belongs to some group!", user, user_id);
            AK_EPI;
            return EXIT_SUCCESS;
        }
        j++;
        AK_free(row);
    }

    if (privilege == 0) {
        printf("User '%s' under ID %d hasn't got any privileges!", user, user_id);
        AK_EPI;
        return EXIT_ERROR;
    }

    AK_EPI;
    return EXIT_SUCCESS;
}

/**
 * @author Jurica Hlevnjak, updated by Lidija Lastavec, updated by Marko Flajšek
 * @brief Function that checks if the group has any privileges. Used in drop group for restriction.
 * @param group name of group
 * @return EXIT_ERROR or EXIT_SUCCESS
 */
int AK_check_group_privilege(char *group) {
    AK_PRO;
    int group_id = AK_group_get_id(group);
    int i = 0;

    if (group_id == EXIT_ERROR) {
        printf("Invalid group name or username!\n");
        AK_EPI;
        return EXIT_ERROR;
    }

    struct list_node *row;
    int privilege = 0;

    while ((row = (struct list_node *) AK_get_row(i, "AK_group_right")) != NULL) {
        struct list_node *group_elem = AK_GetNth_L2(2, row);
        if ((int) *group_elem->data == group_id) {
            privilege = 1;
            printf("Group '%s' under ID %d has some privileges!", group, group_id);
            AK_EPI;
            return EXIT_SUCCESS;
        }
        i++;
        AK_free(row);
    }

    if (privilege == 0) {
        printf("Group '%s' under ID %d hasn't got any privileges!", group, group_id);
        AK_EPI;
        return EXIT_ERROR;
    }

    AK_EPI;
    return EXIT_SUCCESS;
}

/**
 * @author Marko Kir
 * @brief Adds a new role to the AK_role table
 * @param role_name Name of the role to be added
 * @param set_id Optional ID for the role (use 0 for auto-generated ID)
 * @return Role ID or EXIT_ERROR if the role already exists
 */
int AK_role_add(char *role_name, int set_id) {
    AK_PRO;
    char *tblName = "AK_role";
    int roleCheck = AK_role_get_id(role_name);

    if (roleCheck != EXIT_ERROR) {
        printf("Role '%s' already exists!\n", role_name);
        AK_EPI;
        return "exists"; 
    }

    struct list_node *row_root = (struct list_node *) AK_malloc(sizeof(struct list_node));
    AK_Init_L3(&row_root);

    int role_id = AK_get_id();
    if (set_id != 0) role_id = set_id;

    AK_Insert_New_Element(TYPE_INT, &role_id, tblName, "role_id", row_root);
    AK_Insert_New_Element(TYPE_VARCHAR, role_name, tblName, "role_name", row_root);
    AK_insert_row(row_root);

    printf("Added role '%s' under ID %d!\n", role_name, role_id);

    AK_free(row_root);
    AK_EPI;
    return role_id;
}

/**
 * @author Marko Kir
 * @brief Gets the ID of a role by its name
 * @param role_name Name of the role
 * @return Role ID or EXIT_ERROR if the role does not exist
 */
int AK_role_get_id(char *name) {
    AK_PRO;
    int i = 0;
    struct list_node *row;
    AK_PRO;

    while ((row = (struct list_node *) AK_get_row(i, "AK_role")) != NULL) {
        struct list_node *role_name_elem = AK_GetNth_L2(2, row); // Get the role_name column
        if (role_name_elem != NULL && strcmp(role_name_elem->data, name) == 0) {
            struct list_node *role_id_elem = AK_GetNth_L2(1, row); // Get the role_id column
            if (role_id_elem != NULL) {
                int role_id = *((int *)role_id_elem->data); // Safely cast to int
                AK_free(row);
                AK_EPI;
                return role_id;
            }
        }
        i++;
        AK_free(row);
    }

    AK_EPI;
    return EXIT_ERROR;
}

/**
 * @author Marko Kir
 * @brief Removes a role by its name
 * @param role_name Name of the role to be removed
 * @return EXIT_SUCCESS or EXIT_ERROR
 */
int AK_role_remove_by_name(char *role_name) {
    AK_PRO;
    int role_id = AK_role_get_id(role_name);
    if (role_id == EXIT_ERROR) {
        printf("Role '%s' does not exist!\n", role_name);
        return EXIT_ERROR;
    }

    struct list_node *row_root = (struct list_node *) AK_malloc(sizeof(struct list_node));
    AK_Init_L3(&row_root);

    AK_Update_Existing_Element(TYPE_VARCHAR, role_name, "AK_role", "role_name", row_root);
    int result = AK_delete_row(row_root);

    printf("Removed role '%s' under ID %d!\n", role_name, role_id);

    AK_free(row_root);
    return result;
}

/**
 * @author Marko Kir
 * @brief Grants a privilege to a role on a specific table
 * @param role_name Name of the role
 * @param table Name of the table
 * @param right Type of privilege (e.g., "SELECT", "UPDATE", "ALL")
 * @return EXIT_SUCCESS or EXIT_ERROR
 */
 int AK_grant_privilege_role(char *role_name, char *table, char *right) {
    AK_PRO;
    int role_id = AK_role_get_id(role_name);
    int table_id = AK_get_table_obj_id(table);

    if (role_id == EXIT_ERROR || table_id == EXIT_ERROR) {
        printf("Invalid role name or table name!\n");
        return EXIT_ERROR;
    }

    if(strcmp(right, "ALL") == 0) {
        char rights[100];
        strcpy(&rights[0], "UPDATE\0");
        strcpy(&rights[10], "DELETE\0");
        strcpy(&rights[20], "INSERT\0");
        strcpy(&rights[30], "SELECT\0");
        int i;
        for(i = 0; i < 4; i++) {
            struct list_node *row_root = (struct list_node *) AK_malloc(sizeof(struct list_node));
            AK_Init_L3(&row_root);
            int privilege_id = AK_get_id();

            AK_Insert_New_Element(TYPE_INT, &privilege_id, "AK_role_right", "obj_id", row_root);
            AK_Insert_New_Element(TYPE_INT, &role_id, "AK_role_right", "role_id", row_root);
            AK_Insert_New_Element(TYPE_INT, &table_id, "AK_role_right", "artifact_id", row_root);
            AK_Insert_New_Element(TYPE_VARCHAR, &rights[i * 10], "AK_role_right", "right_type", row_root);
            AK_insert_row(row_root);

            AK_free(row_root);
        }
        printf("Granted all privileges for role '%s' under ID %d on table '%s' under ID %d!\n", role_name, role_id, table, table_id);
    } else {
        struct list_node *row_root = (struct list_node *) AK_malloc(sizeof(struct list_node));
        AK_Init_L3(&row_root);

        int privilege_id = AK_get_id();
        AK_Insert_New_Element(TYPE_INT, &privilege_id, "AK_role_right", "obj_id", row_root);
        AK_Insert_New_Element(TYPE_INT, &role_id, "AK_role_right", "role_id", row_root);
        AK_Insert_New_Element(TYPE_INT, &table_id, "AK_role_right", "artifact_id", row_root);
        AK_Insert_New_Element(TYPE_VARCHAR, right, "AK_role_right", "right_type", row_root);
        AK_insert_row(row_root);
        printf("Granted privilege '%s' to role '%s' under ID %d on table '%s' under ID %d!\n", right, role_name, role_id, table, table_id);
    
        AK_free(row_root);
    }
    AK_EPI;
    return EXIT_SUCCESS;
}

/**
 * @author Marko Kir
 * @brief Assigns a role to a user
 * @param username Name of the user
 * @param role_name Name of the role
 * @return EXIT_SUCCESS or EXIT_ERROR
 */
int AK_assign_role_to_user(char *username, char *role_name) {
    AK_PRO;
    int user_id = AK_user_get_id(username);
    int role_id = AK_role_get_id(role_name);

    if (user_id == EXIT_ERROR || role_id == EXIT_ERROR) {
        printf("Invalid username or role name!\n");
        return EXIT_ERROR;
    }

    struct list_node *row_root = (struct list_node *) AK_malloc(sizeof(struct list_node));
    AK_Init_L3(&row_root);

    AK_Insert_New_Element(TYPE_INT, &user_id, "AK_user_role", "user_id", row_root);
    AK_Insert_New_Element(TYPE_INT, &role_id, "AK_user_role", "role_id", row_root);
    AK_insert_row(row_root);

    printf("Assigned role '%s' to user '%s'!\n", role_name, username);

    AK_free(row_root);
    return EXIT_SUCCESS;
}

/**
 * @author Marko Kir
 * @brief Assigns a role to a group
 * @param group_name Name of the group
 * @param role_name Name of the role
 * @return EXIT_SUCCESS or EXIT_ERROR
 */
int AK_assign_role_to_group(char *group_name, char *role_name) {
    AK_PRO;
    int group_id = AK_group_get_id(group_name);
    int role_id = AK_role_get_id(role_name);

    if (group_id == EXIT_ERROR || role_id == EXIT_ERROR) {
        printf("Invalid group name or role name!\n");
        return EXIT_ERROR;
    }

    struct list_node *row_root = (struct list_node *) AK_malloc(sizeof(struct list_node));
    AK_Init_L3(&row_root);

    AK_Insert_New_Element(TYPE_INT, &group_id, "AK_group_role", "group_id", row_root);
    AK_Insert_New_Element(TYPE_INT, &role_id, "AK_group_role", "role_id", row_root);
    AK_insert_row(row_root);

    printf("Assigned role '%s' to group '%s'!\n", role_name, group_name);

    AK_free(row_root);
    return EXIT_SUCCESS;
}

/**
 * @author Marko Kir
 * @brief Checks if a user has a specific privilege via roles
 * @param username Name of the user
 * @param table Name of the table
 * @param privilege Privilege to check
 * @return EXIT_SUCCESS if the user has the privilege, otherwise EXIT_ERROR
 */
int AK_check_user_privilege_via_roles(char *username, char *table, char *privilege) {
    AK_PRO;
    int user_id = AK_user_get_id(username);
    int table_id = AK_get_table_obj_id(table);

    if (user_id == EXIT_ERROR || table_id == EXIT_ERROR) {
        printf("Invalid username or table name!\n");
        return EXIT_ERROR;
    }

    struct list_node *row;
    int i = 0;

    while ((row = (struct list_node *) AK_get_row(i, "AK_user_role")) != NULL) {
        struct list_node *user_elem = AK_GetNth_L2(1, row);
        struct list_node *role_elem = AK_GetNth_L2(2, row);

        // Check data types before comparing
        if (user_elem->type == TYPE_INT && role_elem->type == TYPE_INT) {
            if (*(int *)user_elem->data == user_id) {
                int role_id = *(int *)role_elem->data;

                if (AK_check_role_privilege(role_id, table_id, privilege) == EXIT_SUCCESS) {
                    printf("User '%s' has privilege '%s' on table '%s' via role ID %d.\n",
                           username, privilege, table, role_id);
                    AK_free(row);
                    return EXIT_SUCCESS;
                }
            }
        } else {
            printf("Data type mismatch in AK_user_role table: user_id or role_id is of unexpected type.\n");
        }

        i++;
        AK_free(row);
    }

    printf("User '%s' does not have privilege '%s' on table '%s' via roles!\n", username, privilege, table);
    AK_EPI;
    return EXIT_ERROR;
}

/**
 * @author Marko Kir
 * @brief Checks if a group has a specific privilege via roles
 * @param group_name Name of the group
 * @param table Name of the table
 * @param privilege Privilege to check
 * @return EXIT_SUCCESS if the group has the privilege, otherwise EXIT_ERROR
 */
int AK_check_group_privilege_via_roles(char *group_name, char *table, char *privilege) {
    AK_PRO;
    int group_id = AK_group_get_id(group_name);
    int table_id = AK_get_table_obj_id(table);

    if (group_id == EXIT_ERROR || table_id == EXIT_ERROR) {
        printf("Invalid group name or table name!\n");
        return EXIT_ERROR;
    }

    struct list_node *row;
    int i = 0;

    while ((row = (struct list_node *) AK_get_row(i, "AK_group_role")) != NULL) {
        struct list_node *group_elem = AK_GetNth_L2(1, row);
        struct list_node *role_elem = AK_GetNth_L2(2, row);

        // Check data types before comparing
        if (group_elem->type == TYPE_INT && role_elem->type == TYPE_INT) {
            if (*(int *)group_elem->data == group_id) {
                int role_id = *(int *)role_elem->data;

                if (AK_check_role_privilege(role_id, table_id, privilege) == EXIT_SUCCESS) {
                    printf("Group '%s' has privilege '%s' on table '%s' via role ID %d.\n",
                           group_name, privilege, table, role_id);
                    AK_free(row);
                    return EXIT_SUCCESS;
                }
            }
        } else {
            printf("Data type mismatch in AK_group_role table: group_id or role_id is of unexpected type.\n");
        }

        i++;
        AK_free(row);
    }

    printf("Group '%s' does not have privilege '%s' on table '%s' via roles!\n", group_name, privilege, table);
    AK_EPI;
    return EXIT_ERROR;
}

/**
 * @author Marko Kir
 * @brief Helper function to check if a role has a specific privilege on a table
 * @param role_id ID of the role
 * @param table_id ID of the table
 * @param privilege Privilege to check
 * @return EXIT_SUCCESS if the role has the privilege, otherwise EXIT_ERROR
 */
int AK_check_role_privilege(int role_id, int table_id, char *privilege) {
    AK_PRO;
    struct list_node *row;
    int i = 0;

    while ((row = (struct list_node *) AK_get_row(i, "AK_role_right")) != NULL) {
        struct list_node *role_elem = AK_GetNth_L2(2, row);
        struct list_node *table_elem = AK_GetNth_L2(3, row);
        struct list_node *privilege_elem = AK_GetNth_L2(4, row);

        // Check data types before comparing
        if (role_elem->type == TYPE_INT && table_elem->type == TYPE_INT && 
            privilege_elem->type == TYPE_VARCHAR) {

            if (*(int *)role_elem->data == role_id &&
                *(int *)table_elem->data == table_id &&
                strcmp((char *)privilege_elem->data, privilege) == 0) {
                AK_free(row);
                return EXIT_SUCCESS;
            }
        } else {
            printf("Data type mismatch in AK_role_right table: role_id, table_id, or privilege is of unexpected type.\n");
        }

        i++;
        AK_free(row);
    }

    return EXIT_ERROR;
}

/**
 * @author Marko Kir
 * @brief Revokes a privilege from a role on a specific table, will work when AK_delete_row is fixed
 * @param role_name Name of the role
 * @param table Name of the table
 * @param right Type of privilege (e.g., "SELECT", "UPDATE")
 * @return EXIT_SUCCESS if the privilege is revoked, otherwise EXIT_ERROR
 */
int AK_revoke_privilege_role(char *role_name, char *table, char *right) {
    AK_PRO;
    int role_id = AK_role_get_id(role_name);
    int table_id = AK_get_table_obj_id(table);
    int result = EXIT_ERROR;

    if (role_id == EXIT_ERROR || table_id == EXIT_ERROR) {
        printf("Invalid role name or table name!\n");
        AK_EPI;
        return EXIT_ERROR;
    }

    struct list_node *row_root = (struct list_node *) AK_malloc(sizeof(struct list_node));
    AK_Init_L3(&row_root);

    if (strcmp(right, "ALL") == 0) {
        // Define the array of individual rights
        char rights[4][10] = {"UPDATE", "DELETE", "INSERT", "SELECT"};
        for (int i = 0; i < 4; i++) {
            int i_row = 0;
            struct list_node *row;

            while ((row = (struct list_node *) AK_get_row(i_row, "AK_role_right")) != NULL) {
                struct list_node *role_elem = AK_GetNth_L2(2, row);
                struct list_node *table_elem = AK_GetNth_L2(3, row);
                struct list_node *right_elem = AK_GetNth_L2(4, row);

                if (*(int *)role_elem->data == role_id &&
                    *(int *)table_elem->data == table_id &&
                    strcmp((char *)right_elem->data, rights[i]) == 0) {
                    int obj_id = *(int *)AK_GetNth_L2(1, row)->data;
                    AK_Update_Existing_Element(TYPE_INT, &obj_id, "AK_role_right", "obj_id", row_root);
                    result = AK_delete_row(row_root);
                    if (result == EXIT_SUCCESS) {
                        printf("Revoked privilege '%s' for role '%s' under ID %d on table '%s' under ID %d!\n",
                               rights[i], role_name, role_id, table, table_id);
                    } else {
                        printf("Failed to revoke privilege '%s' for role '%s' under ID %d on table '%s' under ID %d!\n",
                               rights[i], role_name, role_id, table, table_id);
                    }
                }
                i_row++;
                AK_DeleteAll_L3(&row_root);
                AK_free(row);
            }
        }
        printf("Revoked all privileges for group '%s' under ID %d on table '%s'!\n", role_name, role_id, table);    
    } else {
        int i_row = 0;
        struct list_node *row;

        while ((row = (struct list_node *) AK_get_row(i_row, "AK_role_right")) != NULL) {
            struct list_node *role_elem = AK_GetNth_L2(2, row);
            struct list_node *table_elem = AK_GetNth_L2(3, row);
            struct list_node *right_elem = AK_GetNth_L2(4, row);

            if (*(int *)role_elem->data == role_id &&
                *(int *)table_elem->data == table_id &&
                strcmp((char *)right_elem->data, right) == 0) {
                int obj_id = *(int *)AK_GetNth_L2(1, row)->data;
                AK_Update_Existing_Element(TYPE_INT, &obj_id, "AK_role_right", "obj_id", row_root);
                result = AK_delete_row(row_root);
                if (result == EXIT_SUCCESS) {
                    printf("Revoked privilege '%s' for role '%s' under ID %d on table '%s' under ID %d!\n",
                           right, role_name, role_id, table, table_id);
                } else {
                    printf("Failed to revoke privilege '%s' for role '%s' under ID %d on table '%s' under ID %d!\n",
                           right, role_name, role_id, table, table_id);
                }
                break;
            }
            i_row++;
            AK_DeleteAll_L3(&row_root);
            AK_free(row);
        }
    }

    if (result == EXIT_ERROR) {
        printf("Role '%s' does not have privilege '%s' on table '%s'!\n", role_name, right, table);
    }

    AK_free(row_root);
    AK_EPI;
    return result;
}

/**
 * @author Marko Kir
 * @brief Removes a role assigned to a user
 * @param username Name of the user
 * @param role_name Name of the role to be removed
 * @return EXIT_SUCCESS or EXIT_ERROR
 */
int AK_remove_role_from_user(char *username, char *role_name) {
    AK_PRO;
    int user_id = AK_user_get_id(username);
    int role_id = AK_role_get_id(role_name);

    if (user_id == EXIT_ERROR || role_id == EXIT_ERROR) {
        printf("Invalid username or role name!\n");
        return EXIT_ERROR;
    }

    struct list_node *row_root = (struct list_node *) AK_malloc(sizeof(struct list_node));
    AK_Init_L3(&row_root);

    int i = 0;
    struct list_node *row;
    int result = EXIT_ERROR;

    while ((row = (struct list_node *) AK_get_row(i, "AK_user_role")) != NULL) {
        struct list_node *user_elem = AK_GetNth_L2(1, row); // user_id column
        struct list_node *role_elem = AK_GetNth_L2(2, row); // role_id column

        // Check data types before comparing
        if (user_elem->type == TYPE_INT && role_elem->type == TYPE_INT) {
            if (*(int *)user_elem->data == user_id && *(int *)role_elem->data == role_id) {
                AK_Update_Existing_Element(TYPE_INT, &user_id, "AK_user_role", "user_id", row_root);
                result = AK_delete_row(row_root);

                if (result == EXIT_SUCCESS) {
                    printf("Removed role '%s' from user '%s'!\n", role_name, username);
                } else {
                    printf("Failed to remove role '%s' from user '%s'!\n", role_name, username);
                }

                AK_free(row);
                break;
            }
        } else {
            printf("Data type mismatch in AK_user_role table: user_id or role_id is not TYPE_INT.\n");
        }

        AK_free(row);
        i++;
    }

    if (result == EXIT_ERROR) {
        printf("User '%s' does not have role '%s'!\n", username, role_name);
    }

    AK_free(row_root);
    AK_EPI;
    return result;
}

/**
 * @author Marko Kir
 * @brief Removes all roles assigned to a user
 * @param username Name of the user
 * @return EXIT_SUCCESS or EXIT_ERROR
 */
int AK_remove_all_roles_from_user(char *username) {
    AK_PRO;

    int user_id = AK_user_get_id(username);
    if (user_id == EXIT_ERROR) {
        printf("Invalid username!\n");
        AK_EPI;
        return EXIT_ERROR;
    }

    struct list_node *row_root = (struct list_node *) AK_malloc(sizeof(struct list_node));
    AK_Init_L3(&row_root);

    int i = 0;
    struct list_node *row;
    int result = EXIT_SUCCESS;

    while ((row = (struct list_node *) AK_get_row(i, "AK_user_role")) != NULL) {
        struct list_node *user_elem = AK_GetNth_L2(1, row); 

        if (user_elem->type == TYPE_INT && *(int *)user_elem->data == user_id) {
            AK_Update_Existing_Element(TYPE_INT, &user_id, "AK_user_role", "user_id", row_root);
            result = AK_delete_row(row_root);

            if (result == EXIT_ERROR) {
                printf("Failed to remove role for user '%s'!\n", username);
                AK_free(row);
                AK_free(row_root);
                AK_EPI;
                return EXIT_ERROR;
            }
        }

        AK_free(row);
        i++;
    }

    printf("Removed all roles for user '%s'!\n", username);
    AK_free(row_root);
    AK_EPI;
    return EXIT_SUCCESS;
}

/**
 * @author Marko Kir
 * @brief Removes a role assigned to a group
 * @param group_name Name of the group
 * @param role_name Name of the role to be removed
 * @return EXIT_SUCCESS or EXIT_ERROR
 */
int AK_remove_role_from_group(char *group_name, char *role_name) {
    AK_PRO;
    int group_id = AK_group_get_id(group_name);
    int role_id = AK_role_get_id(role_name);

    if (group_id == EXIT_ERROR || role_id == EXIT_ERROR) {
        printf("Invalid group name or role name!\n");
        return EXIT_ERROR;
    }

    struct list_node *row_root = (struct list_node *) AK_malloc(sizeof(struct list_node));
    AK_Init_L3(&row_root);

    int i = 0;
    struct list_node *row;
    int result = EXIT_ERROR;

    while ((row = (struct list_node *) AK_get_row(i, "AK_group_role")) != NULL) {
        struct list_node *group_elem = AK_GetNth_L2(1, row); // group_id column
        struct list_node *role_elem = AK_GetNth_L2(2, row); // role_id column

        // Check data types before comparing
        if (group_elem->type == TYPE_INT && role_elem->type == TYPE_INT) {
            if (*(int *)group_elem->data == group_id && *(int *)role_elem->data == role_id) {
                AK_Update_Existing_Element(TYPE_INT, &group_id, "AK_group_role", "group_id", row_root);
                result = AK_delete_row(row_root);

                if (result == EXIT_SUCCESS) {
                    printf("Removed role '%s' from group '%s'!\n", group_name, group_name);
                } else {
                    printf("Failed to remove role '%s' from group '%s'!\n", role_name, group_name);
                }

                AK_free(row);
                break;
            }
        } else {
            printf("Data type mismatch in AK_group_role table: group_id or role_id is not TYPE_INT.\n");
        }

        AK_free(row);
        i++;
    }

    if (result == EXIT_ERROR) {
        printf("Group '%s' does not have role '%s'!\n", group_name, role_name);
    }

    AK_free(row_root);
    AK_EPI;
    return result;
}

/**
 * @author Marko Kir
 * @brief Removes all roles assigned to a group
 * @param group_name Name of the group
 * @return EXIT_SUCCESS or EXIT_ERROR
 */
int AK_remove_all_roles_from_group(char *group_name) {
    AK_PRO;

    int group_id = AK_group_get_id(group_name);
    if (group_id == EXIT_ERROR) {
        printf("Invalid group name!\n");
        AK_EPI;
        return EXIT_ERROR;
    }

    struct list_node *row_root = (struct list_node *) AK_malloc(sizeof(struct list_node));
    AK_Init_L3(&row_root);

    int i = 0;
    struct list_node *row;
    int result = EXIT_SUCCESS;

    while ((row = (struct list_node *) AK_get_row(i, "AK_group_role")) != NULL) {
        struct list_node *group_elem = AK_GetNth_L2(1, row); 

        if (group_elem->type == TYPE_INT && *(int *)group_elem->data == group_id) {
            AK_Update_Existing_Element(TYPE_INT, &group_id, "AK_group_role", "group_id", row_root);
            result = AK_delete_row(row_root);

            if (result == EXIT_ERROR) {
                printf("Failed to remove role for group '%s'!\n", group_name);
                AK_free(row);
                AK_free(row_root);
                AK_EPI;
                return EXIT_ERROR;
            }
        }

        AK_free(row);
        i++;
    }

    printf("Removed all roles for group '%s'!\n", group_name);
    AK_free(row_root);
    AK_EPI;
    return EXIT_SUCCESS;
}

/**
 * @author Kristina Takač, updated by Tomislav Ilisevic, updated by Lidija Lastavec, updated by Marko Flajšek
 * @brief Function that tests all the previous functions
 * @return no return value                                                     
 */
TestResult AK_privileges_test() {
    AK_PRO;

    int successful[31] = {0};

    printf("\n\n");
    printf("||====================================================================|| \n");
    printf("||////////////////////////////////////////////////////////////////////|| \n");
    printf("||========================| privileges.c Tests |======================|| \n");
    printf("||////////////////////////////////////////////////////////////////////|| \n");
    printf("||====================================================================|| \n");


    /******************/
    /* 1. AK_user_add */
    /******************/

    printf("\n1. Test - AK_user_add function - Inserts a new user in the AK_user table\n");
    printf("\nTest data: user1 1111; user2 2222; user3 3333; user4 4444;\n\n");
    printf("Result:\n\n");

    if (AK_user_add("user1", "1111", NEW_ID) == EXIT_ERROR) {
    printf("Test 1. - Fail!\n\n");
    } else {
    printf("Test 1. - Pass!\n\n");
    successful[0] = 1;
    }

    //adding 3 more users for future tests
    AK_user_add("user2", "2222", NEW_ID);
    AK_user_add("user3", "3333", NEW_ID);
    AK_user_add("user4", "4444", NEW_ID);

    printf("\n");
    AK_print_table("AK_user");

    printf("\n\n||====================================================================|| \n");


    /*********************/
    /* 2. AK_user_rename */
    /*********************/

    printf("\n\n2. Test - AK_user_rename function - Renames a user in the AK_user table\n");
    printf("\nTest data: rename user4 to user5\n\n");
    printf("Result:\n\n");

    if (AK_user_rename("user4", "user5", "4444") == EXIT_ERROR) {
        printf("\nTest 2. - Fail!\n");
    } else {
        printf("\nTest 2. - Pass!\n");
        successful[1] = 1;
    }

    printf("\n");
    AK_print_table("AK_user");

    printf("\n\n||====================================================================|| \n");


    /*****************************/
    /* 3. AK_user_remove_by_name */
    /*****************************/

    printf("\n\n3. Test - AK_user_remove_by_name function - Removes a user by name from the AK_user table\n");
    printf("\nTest data: remove user5\n\n");
    printf("Result:\n");

    if (AK_user_remove_by_name("user5") == EXIT_ERROR) {
        printf("\nTest 3. - Fail!\n");
    } else {
        printf("\nTest 3. - Pass!\n");
        successful[2] = 1;
    }

    printf("\n");
    AK_print_table("AK_user");

    printf("\n\n||====================================================================|| \n");


    /*******************/
    /* 4. AK_group_add */
    /*******************/

    printf("\n\n4. Test - AK_group_add function - Adds a new group in the AK_group table\n");
    printf("\nTest data: group1, group2, group3, group4\n\n");
    printf("Result:\n\n");

    if (AK_group_add("group1", NEW_ID) == EXIT_ERROR) {
        printf("\nTest 4. - Fail!\n\n");
    } else {
        printf("\nTest 4. - Pass!\n\n");
        successful[3] = 1;
    }

    //adding 3 more groups for future tests
    AK_group_add("group2", NEW_ID);
    AK_group_add("group3", NEW_ID);
    AK_group_add("group4", NEW_ID);

    printf("\n");
    AK_print_table("AK_group");

    printf("\n\n||====================================================================|| \n");


    /**********************/
    /* 5. AK_group_rename */
    /**********************/

    printf("\n\n5. Test - AK_group_rename function - Renames a group\n");
    printf("\nTest data: rename group4 to group5\n\n");
    printf("Result:\n\n");

    if (AK_group_rename("group4", "group5") == EXIT_ERROR) {
        printf("\nTest 5. - Fail!\n\n");
    } else {
        printf("\nTest 5. - Pass!\n\n");
        successful[4] = 1;
    }

    AK_print_table("AK_group");

    printf("\n\n||====================================================================|| \n");


    /******************************/
    /* 6. AK_group_remove_by_name */
    /******************************/

    printf("\n\n6. Test - AK_group_remove_by_name function - Remove a group by name from the AK_group table\n");
    printf("\nTest data: group5\n");

    if (AK_group_remove_by_name("group5") == EXIT_ERROR) {
        printf("\nTest 6. - Fail!\n\n");
    } else {
        printf("\nTest 6. - Pass!\n\n");
        successful[5] = 1;
    }

    AK_print_table("AK_group");

    printf("\n\n||====================================================================|| \n");


    /***************************/
    /* 7. AK_add_user_to_group */
    /***************************/

    printf("\n\n7. Test - AK_add_user_to_group function - Puts a user into the given group\n");
    printf("\nTest data: add user1 to group1; user2 to group1; user2 to group2, user2 to group3\n\n");
    printf("Result:\n\n");

    if (AK_add_user_to_group("user1", "group1") == EXIT_ERROR) {
        printf("\nTest 7. - Fail!\n\n");
    } else {
        printf("\nTest 7. - Pass!\n\n");
        successful[6] = 1;
    }

    //adding 1 more user to a group for future tests
    AK_add_user_to_group("user2", "group1");

    printf("\n");
    AK_print_table("AK_user_group");

    printf("\n\n||====================================================================|| \n");


    /****************************************/
    /* 8. AK_grant_privilege_group function */
    /****************************************/

    printf("\n\n8. Test - AK_grant_privilege_group function - Grants privilege to a given group on a given table\n");
    printf("\nTest data: grant all privileges to group1 on professor table\n\n");
    printf("Result:\n\n");

    if (AK_grant_privilege_group("group1", "professor", "ALL") == EXIT_ERROR) {
        printf("Test 8. - Fail!\n\n");
    } else {
        printf("Test 8. - Pass!\n\n");
        successful[7] = 1;
    }
    
    //running 4 more functions for future tests
    AK_grant_privilege_group("group1", "student", "ALL");
    AK_grant_privilege_group("group2", "student", "ALL");
    AK_grant_privilege_group("group2", "professor", "UPDATE");
    AK_grant_privilege_group("group2", "professor2", "DELETE");

    printf("\n");
    AK_print_table("AK_group_right");

    printf("\n\n||====================================================================|| \n");


    /********************************/
    /* 9. AK_revoke_privilege_group */
    /********************************/

    /* NOTICE: This test isn't currently revoking a privilege since the obj_id in the AK_group_right table is passing the value of 127.
     Once the issue #87 on GitHub concerning the data type is solved, the test should be working as expected. */
    
    printf("\n\n9. Test - AK_revoke_privilege_group function - Revokes privilege for a group on a given table\n");
    printf("\nTest data: revoke UPDATE privilege for group1 on the professor table\n\n");
    printf("Result:\n\n");

    if (AK_revoke_privilege_group("group1", "professor", "UPDATE") == EXIT_ERROR) {
        printf("\nTest 9. - Fail!\n\n");
    } else {
        printf("\nTest 9. - Pass!\n\n");
        successful[8] = 1;
    }

    AK_revoke_privilege_group("group2", "student", "ALL");

    AK_print_table("AK_group_right");

    printf("\n\n||====================================================================|| \n");


    /**************************************/
    /* 10. AK_revoke_all_privileges_group */
    /**************************************/


    printf("\n\n10. Test - AK_revoke_all_privileges_group function - Revokes ALL privileges for a group on ALL tables\n");
    printf("\nTest data: revoke all privileges for group1 on all tables\n\n");
    printf("Result:\n\n");

    if (AK_revoke_all_privileges_group("group1") == EXIT_ERROR) {
        printf("Test 10. - Fail!\n\n");
    } else {
        printf("Test 10. - Pass!\n\n");
        successful[9] = 1;
    }

    AK_print_table("AK_group_right");

    printf("\n\n||====================================================================|| \n");


    /*******************************/
    /* 11. AK_grant_privilege_user */
    /*******************************/

    printf("\n\n11. Test - AK_grant_privilege_user function - Grants privilege to a given user on a given table\n");
    printf("\nTest data: grant privileges for users user1, user2 and user3 on tables student and professor\n\n");
    printf("Result:\n\n");

    if (AK_grant_privilege_user("user1", "student", "UPDATE") == EXIT_ERROR) {
        printf("Test 11. - Fail!\n\n");
    } else {
        printf("Test 11. - Pass!\n\n");
        successful[10] = 1;
    }

    //adding 7 more privileges for future tests    
    AK_grant_privilege_user("user1", "student", "DELETE");
    AK_grant_privilege_user("user1", "student", "SELECT");
    AK_grant_privilege_user("user1", "professor", "ALL");
    AK_grant_privilege_user("user2", "student", "UPDATE");
    AK_grant_privilege_user("user2", "professor", "SELECT");
    AK_grant_privilege_user("user3", "professor", "SELECT");
    AK_grant_privilege_user("user3", "student", "SELECT");

    printf("\n");
    AK_print_table("AK_user_right");

    printf("\n\n||====================================================================|| \n");


    /********************************/
    /* 12. AK_revoke_privilege_user */
    /********************************/
    
    /* NOTICE: This test isn't currently revoking a privilege since the obj_id in the AK_user_right is passing the value of 127.
     Once the issue #87 on GitHub concerning the data type is solved, the test should be working as expected. */

    printf("\n\n12. Test - AK_revoke_privilege_user function - Revokes user's privilege(s) on a given table\n");
    printf("\nTest data: revoke DELETE privilege for user1 on the 'student' table\n\n");
    printf("Result:\n\n");

    if (AK_revoke_privilege_user("user1", "student", "DELETE") == EXIT_ERROR) {
        printf("\nTest 12. - Fail!\n\n");
    } else {
        printf("\nTest 12. - Pass!\n\n");
        successful[11] = 1;
    }

    AK_revoke_privilege_user("user1", "professor", "ALL");

    AK_print_table("AK_user_right");

    printf("\n\n||====================================================================|| \n");


    /*************************************/
    /* 13. AK_revoke_all_privileges_user */
    /*************************************/

    printf("\n\n13. Test - AK_revoke_all_privileges_user function - Revokes ALL user's privileges on ALL tables\n");
    printf("\nTest data: revoke user3's privileges on all tables\n\n");
    printf("Result:\n\n");

    if (AK_revoke_all_privileges_user("user3") == EXIT_ERROR) {
        printf("\nTest 13. - Fail!\n\n");
    } else {
        printf("\nTest 13. - Pass!\n\n");
        successful[12] = 1;
    }

    AK_print_table("AK_user_right");

    printf("\n\n||====================================================================|| \n");


    /**************************/
    /* 14. AK_check_privilege */
    /**************************/

    printf("\n\n14. Test - AK_check_privilege function - Checks whether the given user has the right for the given operation on a desired table\n");
    printf("\nTest data: check if user user2 has the right to UPDATE data in the STUDENT table\n\n");
    printf("Result:\n\n");

    if (AK_check_privilege("user2", "student", "UPDATE") == EXIT_ERROR) {
        printf("\n\nTest 14. - Fail!\n\n");
    } else {
        printf("\n\nTest 14. - Pass!\n\n");
        successful[13] = 1;
    }

    //test for user without a right
    printf("\nTest data: check if user user1 has right to INSERT data into the PROFESSOR2 table\n\n");
    printf("Result:\n\n");

    if (AK_check_privilege("user1", "professor2", "INSERT") == EXIT_ERROR) {
        printf("\n\nTest 14. - Pass!\n");
    } else {
        printf("\n\nTest 14. - Fail!\n");
    }

    printf("\n");
    AK_print_table("AK_user_right");

    printf("\n\n||====================================================================|| \n");


    /********************************/
    /* 15. AK_check_group_privilege */
    /********************************/

    printf("\n\n15. Test - AK_check_group_privilege function - Checks whether the given group has any privileges\n");
    printf("\nTest data: check if group 2 has privileges\n\n");
    printf("Result:\n\n");

    if (AK_check_group_privilege("group2") == EXIT_SUCCESS) {
        printf("\n\nTest 15. - Pass!\n\n");
        successful[14] = 1;
    } else {
        printf("\n\nTest 15. - Fail!\n\n");
    }

    //test for a group without privilege
    printf("\nTest data: group3 - without privileges\n\n");
    printf("Result:\n\n");

    if (AK_check_group_privilege("group3") == EXIT_ERROR) {
        printf("\n\nTest 15. - Pass!\n");
    } else {
        printf("\n\nTest 15. - Fail!\n");
    }

    printf("\n");
    AK_print_table("AK_group_right");

    printf("\n\n||====================================================================|| \n");


    /*******************************/
    /* 16. AK_check_user_privilege */
    /*******************************/

    printf("\n\n16. Test - AK_check_user_privilege function - Checks if the given user has any privileges or belongs to any group\n");
    printf("\nTest data: check if user user1 has ANY privileges\n\n");
    printf("Result:\n\n");

    if (AK_check_user_privilege("user1") == EXIT_ERROR) {
        printf("\n\nTest 16. - Fail!\n\n");
    } else {
        printf("\n\nTest 16. - Pass!\n\n");
        successful[15] = 1;
    }

    //test for a user without any privileges
    printf("\nTest data: check for user without privileges (user3)\n\n");
    printf("Result:\n\n");

    if (AK_check_user_privilege("user3") == EXIT_ERROR) {
        printf("\n\nTest 16. - Pass!\n");
    } else {
        printf("\n\nTest 16. - Fail!\n");
    }

    printf("\n");
    AK_print_table("AK_user_right");
    printf("\n\n");
    AK_print_table("AK_group_right");

    printf("\n\n||====================================================================|| \n");


    /**************************************/
    /* 17. AK_remove_user_from_all_groups */
    /**************************************/

    printf("\n\n17. Test - AK_remove_user_from_all_groups function - Removes the given user from all groups\n");
    printf("\nTest data: remove user1 (member of group1)\n\n");

    AK_print_table("AK_user_group");

    printf("\nResult:\n\n");

    if (AK_remove_user_from_all_groups("user1") == EXIT_SUCCESS) {
        printf("\n\nTest 17. - Pass!\n");
        successful[16] = 1;
    } else {
        printf("\n\nTest 17. - Fail!\n");
    }

    printf("\n");
    AK_print_table("AK_user_group");

    printf("\n\n||====================================================================|| \n");


    /**************************************/
    /* 18. AK_remove_all_users_from_group */
    /**************************************/

    printf("\n\n18. Test - AK_remove_all_users_from_group function - Removes all users from the given group\n");
    printf("\nTest data: remove users from group1 (with added users)\n\n");

    AK_print_table("AK_user_group");

    printf("\nResult:\n\n");

    if (AK_remove_all_users_from_group("group1") == EXIT_SUCCESS) {
        printf("\n\nTest 18. - Pass!\n");
        successful[17] = 1;
    } else {
        printf("\n\nTest 18. - Fail!\n");
    }

    printf("\n");
    AK_print_table("AK_user_group");

    printf("\n\n||====================================================================|| \n");


    /**************************************/
    /* 19. AK_user_check_pass */
    /**************************************/

    printf("\n19. Test - AK_user_check_pass function - Checks if given username and matching password exists in database\n");
    printf("Result:\n\n");


    if (AK_user_check_pass("user2", "2222") == 0) {
        printf("\n\nTest 19. - Fail!\n");
    } else {
        printf("\n\nTest 19. - Pass!\n");
        successful[18] = 1;
    }

    printf("\n");
    AK_print_table("AK_user");

    printf("\n\n||====================================================================|| \n");

    /******************/
    /* 20. AK_role_add */
    /******************/

    printf("\n20. Test - AK_role_add function - Adds a new role to the AK_role table\n");
    printf("\nTest data: role1, role2, role3, role4\n\n");
    printf("Result:\n\n");

    if (AK_role_add("role1", NEW_ID) == EXIT_ERROR) {
        printf("\nTest 20. - Fail!\n\n");
    } else {
        printf("\nTest 20. - Pass!\n\n");
        successful[19] = 1;
    }

    // Adding 3 more roles for future tests
    AK_role_add("role2", NEW_ID);
    AK_role_add("role3", NEW_ID);
    AK_role_add("role4", NEW_ID);

    printf("\n");
    AK_print_table("AK_role");

    printf("\n\n||====================================================================|| \n");

    /************************/
    /* 21. AK_role_get_id */
    /************************/

    printf("\n21. Test - AK_role_get_id function - Retrieves the ID of a role by its name\n");
    printf("\nTest data: role1\n\n");
    printf("Result:\n\n");

    int role_id = AK_role_get_id("role1"); // Store the returned value

    if (role_id == EXIT_ERROR) {
        printf("\nTest 21. - Fail! Returned value: %d\n\n", role_id);
    } else {
        printf("\nTest 21. - Pass! Returned value: %d\n\n", role_id);
        successful[20] = 1;
    }

    printf("\n\n||====================================================================|| \n");

    /*******************************/
    /* 22. AK_role_remove_by_name */
    /*******************************/

    printf("\n22. Test - AK_role_remove_by_name function - Removes a role by its name\n");
    printf("\nTest data: role4\n\n");
    printf("Result:\n\n");

    if (AK_role_remove_by_name("role4") == EXIT_ERROR) {
        printf("\nTest 22. - Fail!\n\n");
    } else {
        printf("\nTest 22. - Pass!\n\n");
        successful[21] = 1;
    }

    printf("\n");
    AK_print_table("AK_role");

    printf("\n\n||====================================================================|| \n");

    /************************************/
    /* 23. AK_grant_privilege_role */
    /************************************/

    printf("\n23. Test - AK_grant_privilege_role function - Grants a privilege to a role on a specific table\n");
    printf("\nTest data: grant SELECT privilege to role1 on student\n\n");
    printf("Result:\n\n");

    if (AK_grant_privilege_role("role1", "student", "SELECT") == EXIT_ERROR) {
        printf("\nTest 23. - Fail!\n\n");
    } else {
        printf("\nTest 23. - Pass!\n\n");
        successful[22] = 1;
    }

    AK_grant_privilege_role("role1", "student", "UPDATE");
    AK_grant_privilege_role("role2", "student", "UPDATE");
    AK_grant_privilege_role("role3", "professor", "ALL");

    printf("\n");
    AK_print_table("AK_role_right");

    printf("\n\n||====================================================================|| \n");

    /************************************/
    /* 24. AK_revoke_privilege_role */
    /************************************/

    printf("\n24. Test - AK_revoke_privilege_role function - Revokes a privilege from a role on a specific table\n");
    printf("\nTest data: revoke SELECT privilege from role1 on student\n\n");
    printf("Result:\n\n");

    if (AK_revoke_privilege_role("role1", "student", "SELECT") == EXIT_ERROR) {
        printf("\nTest 24. - Fail!\n\n");
    } else {
        printf("\nTest 24. - Pass!\n\n");
        successful[23] = 1;
    }

    AK_revoke_privilege_role("role3", "professor", "ALL");

    printf("\n");
    AK_print_table("AK_role_right");

    printf("\n\n||====================================================================|| \n");

    /************************************/
    /* 25. AK_assign_role_to_user */
    /************************************/

    printf("\n25. Test - AK_assign_role_to_user function - Assigns a role to a user\n");
    printf("\nTest data: assign role1 to user1\n\n");
    printf("Result:\n\n");

    if (AK_assign_role_to_user("user1", "role1") == EXIT_ERROR) {
        printf("\nTest 25. - Fail!\n\n");
    } else {
        printf("\nTest 25. - Pass!\n\n");
        successful[24] = 1;
    }

    AK_assign_role_to_user("user2", "role2");
    AK_assign_role_to_user("user3", "role1");
    AK_assign_role_to_user("user3", "role2");

    printf("\n");
    AK_print_table("AK_user_role");

    printf("\n\n||====================================================================|| \n");

    /************************************/
    /* 26. AK_remove_role_from_user */
    /************************************/

    printf("\n26. Test - AK_remove_role_from_user function - Removes a role assigned to a user\n");
    printf("\nTest data: remove role1 from user1\n\n");
    printf("Result:\n\n");

    if (AK_remove_role_from_user("user2", "role2") == EXIT_ERROR) {
        printf("\nTest 26. - Fail!\n\n");
    } else {
        printf("\nTest 26. - Pass!\n\n");
        successful[25] = 1;
    }

    printf("\n");
    AK_print_table("AK_user_role");

    printf("\n\n||====================================================================|| \n");

    /********************************************/
    /* 27. AK_check_user_privilege_via_roles */
    /********************************************/

    printf("\n27. Test - AK_check_user_privilege_via_roles function - Checks if a user has a specific privilege via roles\n");
    printf("\nTest data: check if user1 has UPDATE privilege on student via roles\n\n");
    printf("Result:\n\n");

    if (AK_check_user_privilege_via_roles("user1", "student", "UPDATE") == EXIT_ERROR) {
        printf("\nTest 27. - Fail!\n\n");
    } else {
        printf("\nTest 27. - Pass!\n\n");
        successful[26] = 1;
    }

    printf("\n\n||====================================================================|| \n");

    /********************************************/
    /* 28. AK_assign_role_to_group */
    /********************************************/

    printf("\n28. Test - AK_assign_role_to_group function - Assigns a role to a group\n");
    printf("\nTest data: assign role1 to group1\n\n");
    printf("Result:\n\n");

    if (AK_assign_role_to_group("group1", "role1") == EXIT_ERROR) {
        printf("\nTest 28. - Fail!\n\n");
    } else {
        printf("\nTest 28. - Pass!\n\n");
        successful[27] = 1;
    }

    AK_assign_role_to_group("group2", "role2");
    AK_assign_role_to_group("group3", "role1");
    AK_assign_role_to_group("group3", "role2");

    printf("\n");
    AK_print_table("AK_group_role");

    printf("\n\n||====================================================================|| \n");

    /********************************************/
    /* 29. AK_remove_role_from_group */
    /********************************************/

    printf("\n29. Test - AK_remove_role_from_group function - Removes a role from a group\n");
    printf("\nTest data: remove role2 from group2\n\n");
    printf("Result:\n\n");

    if (AK_remove_role_from_group("group2", "role2") == EXIT_ERROR) {
        printf("\nTest 29. - Fail!\n\n");
    } else {
        printf("\nTest 29. - Pass!\n\n");
        successful[28] = 1;
    }

    printf("\n");
    AK_print_table("AK_group_role");

    printf("\n\n||====================================================================|| \n");

    /********************************************/
    /* 30. AK_check_group_privilege_via_roles */
    /********************************************/

    printf("\n30. Test - AK_check_group_privilege_via_roles function - Checks if a group has a specific privilege via roles\n");
    printf("\nTest data: check if group1 has UPDATE privilege on student via roles\n\n");
    printf("Result:\n\n");

    if (AK_check_group_privilege_via_roles("group1", "student", "UPDATE") == EXIT_ERROR) {
        printf("\nTest 30. - Fail!\n\n");
    } else {
        printf("\nTest 30. - Pass!\n\n");
        successful[29] = 1;
    }

    printf("\n\n||====================================================================|| \n");

    /********************************************/
    /* 31. AK_remove_all_roles_from_user */
    /********************************************/

    printf("\n31. Test - AK_remove_all_roles_from_user function - Removes all roles assigned to a user\n");
    printf("\nTest data: remove all roles assigned to user1\n\n");
    printf("Result:\n\n");

    if (AK_remove_all_roles_from_user("user3") == EXIT_ERROR) {
        printf("\nTest 31. - Fail!\n\n");
    } else {
        printf("\nTest 31. - Pass!\n\n");
        successful[30] = 1;
    }

    printf("\n");
    AK_print_table("AK_user_role");

    printf("\n\n||====================================================================|| \n");

    /********************************************/
    /* 32. AK_remove_all_roles_from_group */
    /********************************************/

    printf("\n32. Test - AK_remove_all_roles_from_group function - Removes all roles assigned to a group\n");
    printf("\nTest data: remove all roles assigned to group1\n\n");
    printf("Result:\n\n");

    if (AK_remove_all_roles_from_group("group3") == EXIT_ERROR) {
        printf("\nTest 32. - Fail!\n\n");
    } else {
        printf("\nTest 32. - Pass!\n\n");
        successful[31] = 1;
    }

    printf("\n");
    AK_print_table("AK_group_role");

    printf("\n\n||====================================================================|| \n");

    /* END SUMMARY*/

    printf("\nSummary: \n");
    int num = 0;
    int numFail = 0;
    for (num = 0; num < 32; num++) {
        printf("%i. Test: %s \n", (num + 1), (successful[num] == 1 ? "Pass" : "Fail"));
        if (successful[num] == 0) numFail++;
    }

    if (numFail == 0) printf("\nALL TESTS PASSED!\n");
    else printf("\nNumber of failed tests: %d\n", numFail);

    AK_EPI;

    return TEST_result(num-numFail,numFail);
}