void queue_add_client(client_t *client)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) 
    {
        if (!clients[i])
        {   
            clients[i] = client;
            break;
        }
    } 

    pthread_mutex_unlock(&clients_mutex);
}

void queue_remove_client(int uid)
{
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i])
        {
            if (clients[i]->uid == uid)
            {
                clients[i] = NULL;
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void queue_add_room(room_t *room)
{
    pthread_mutex_lock(&rooms_mutex);

    for (int i = 0; i < MAX_ROOMS; i++) 
    {
        if (!rooms[i])
        {   
            rooms[i] = room;
            break;
        }
    } 

    pthread_mutex_unlock(&rooms_mutex);
}

void queue_remove_room(int uid)
{
    pthread_mutex_lock(&rooms_mutex);

    for (int i = 0; i < MAX_ROOMS; i++)
    {
        if (rooms[i])
        {
            if (rooms[i]->uid == uid)
            {
                rooms[i] = NULL;
            }
        }
    }

    pthread_mutex_unlock(&rooms_mutex);
}
